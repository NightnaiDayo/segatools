#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hook/table.h"
#include "platform/epay.h"
#include "platform/icasrw.h"
#include "util/dprintf.h"

/* Recovered from aime_rw_adapterMD 1.0.3 (ef266a2e). These are adapter
 * statuses, not HRESULTs. Never return success without filling output sinks. */
#define ICAS_INVALID 0x10001u
#define ICAS_NOT_OPEN 0x10002u
#define ICAS_DEVICE_ERROR 0x20001u
#define ICAS_TIMEOUT 0x20002u
#define ICAS_CANCELLED 0x20003u

/* The client owns these output buffers. Slot 0 is reserved by the C++ ABI;
 * write copies bytes into the sink and reports whether they were accepted. */
struct icas_sink {
    const struct icas_sink_vtable *vtable;
};
struct icas_sink_vtable {
    void *reserved;
    bool (*write)(struct icas_sink *, const uint8_t *, size_t);
};

/* TFPS 1.0.3 reader ABI. Keep the method order, including both destructor
 * entries: the real payment client calls these methods through its vtable. */
struct icas_reader;
struct icas_vtable {
    uint32_t (*configure)(struct icas_reader *, const char *);
    uint32_t (*claim)(struct icas_reader *);
    uint32_t (*release)(struct icas_reader *);
    uint32_t (*open_felica)(struct icas_reader *);
    uint32_t (*close_felica)(struct icas_reader *);
    uint32_t (*transact)(struct icas_reader *, uint32_t, uint32_t,
            const uint8_t *, size_t, struct icas_sink *);
    uint32_t (*open_nfc)(struct icas_reader *, uint8_t, uint16_t);
    uint32_t (*close_nfc)(struct icas_reader *);
    uint32_t (*detect)(struct icas_reader *, uint32_t, uint32_t *,
            struct icas_sink *, struct icas_sink *);
    uint32_t (*reserved)(struct icas_reader *);
    uint32_t (*transact_retry)(struct icas_reader *, uint32_t, uint32_t,
            const uint8_t *, size_t, struct icas_sink *);
    uint32_t (*led)(struct icas_reader *, uint32_t, uint32_t, uint32_t,
            const uint8_t *, size_t);
    void (*cancel)(struct icas_reader *);
    void (*reset_cancel)(struct icas_reader *);
    const char *(*error)(struct icas_reader *);
    void (*destroy)(struct icas_reader *);
    void *(*deleting_destructor)(struct icas_reader *, unsigned int);
};
struct icas_reader {
    const struct icas_vtable *vtable;
    /* Cancellation can arrive from another thread while detect is waiting. */
    volatile LONG cancelled;
    bool claimed;
    unsigned int mode; /* 0: closed, 1: FeliCa, 2: generic NFC */
    uint16_t card_types;
    uint8_t idm[8];
};

static struct epay_config icas_cfg;
static HMODULE icas_adapter;
static HMODULE icas_adapter_debug;
static HRESULT (*icas_aimeio_poll)(uint64_t *);
static HRESULT (*icas_aimeio_access_poll)(uint8_t *);

/* Resolve the bridge from the loaded game hook without depending on its DLL
 * name. The hook owns AimeIO initialization and serializes reader access. */
static bool icas_find_aimeio(void)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    MODULEENTRY32W entry = {0};
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry)) do {
        if (icas_cfg.icas_aime_access_code) {
            icas_aimeio_access_poll = (void *) GetProcAddress(entry.hModule,
                    "segatools_icas_aimeio_poll_access_code");
            if (icas_aimeio_access_poll) break;
        } else {
            icas_aimeio_poll = (void *) GetProcAddress(entry.hModule,
                    "segatools_icas_aimeio_poll");
            if (icas_aimeio_poll) break;
        }
    } while (Module32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    return icas_cfg.icas_aime_access_code
            ? icas_aimeio_access_poll != NULL : icas_aimeio_poll != NULL;
}

static FARPROC (WINAPI *next_GetProcAddress)(HMODULE, LPCSTR);
static FARPROC WINAPI icas_GetProcAddress(HMODULE module, LPCSTR name);
static uint32_t (*next_set_rw_adapter)(void *, uint32_t,
        struct icas_reader *, const char *);
static uint32_t (*next_payment_initialize)(void *, uint64_t);
static HMODULE icas_payment;

static const struct hook_symbol icas_symbols[] = {
    {
        .name = "GetProcAddress",
        .patch = icas_GetProcAddress,
        .link = (void **) &next_GetProcAddress,
    },
};

/* AimeIO provides ten packed-BCD bytes. Hash exactly 20 ASCII digits, keeping
 * leading zeroes. Amusement IC codes contain a 3-digit company code, 12-digit
 * encoded card data, and 5-digit checksum. Keep the company namespace and all
 * card data; the checksum adds no independent entropy. Use all 64 output bits
 * without a fixed prefix. Both file and hardware inputs use this mapping. */
static bool icas_access_code_idm(const uint8_t code[10], uint8_t idm[8])
{
    uint8_t digits[20], digest[32];
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    bool ok = false;
    for (unsigned int i = 0; i < 10; i++) {
        if ((code[i] >> 4) > 9 || (code[i] & 15) > 9) return false;
        digits[i * 2] = '0' + (code[i] >> 4);
        digits[i * 2 + 1] = '0' + (code[i] & 15);
    }
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) goto end;
    if (BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0) < 0) goto end;
    if (BCryptHashData(hash, digits, sizeof(digits), 0) < 0) goto end;
    if (BCryptFinishHash(hash, digest, sizeof(digest), 0) < 0) goto end;
    memcpy(idm, digest, 8);
    ok = true;
end:
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    SecureZeroMemory(digits, sizeof(digits));
    SecureZeroMemory(digest, sizeof(digest));
    return ok;
}

/* Read eight hex IDm bytes or ten packed-decimal access-code bytes.
 * Whitespace is allowed, but malformed/trailing data is rejected. */
static bool icas_read_card(uint8_t idm[8])
{
    bool access = icas_cfg.icas_aime_access_code;
    unsigned int expected = access ? 10 : 8;
    FILE *f = _wfopen(access ? icas_cfg.icas_aime_path : icas_cfg.icas_card_path, L"r");
    unsigned int count = 0;
    int c;
    int high = -1;
    uint8_t bytes[10];
    bool valid = true;

    if (f == NULL) {
        return false;
    }
    while ((c = fgetc(f)) != EOF) {
        int digit;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            continue;
        }
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else { valid = false; break; }
        if ((access && digit > 9) || count >= expected) { valid = false; break; }
        if (high < 0) high = digit;
        else {
            bytes[count++] = (uint8_t) ((high << 4) | digit);
            high = -1;
        }
    }
    valid = valid && !ferror(f) && count == expected && high < 0;
    fclose(f);
    if (valid && access) valid = icas_access_code_idm(bytes, idm);
    else if (valid) memcpy(idm, bytes, 8);
    SecureZeroMemory(bytes, sizeof(bytes));
    return valid;
}

static uint32_t icas_configure(struct icas_reader *self, const char *connection)
{
    (void) self;
    return connection != NULL ? 0 : ICAS_INVALID;
}
static uint32_t icas_claim(struct icas_reader *self)
{
    self->claimed = true;
    InterlockedExchange(&self->cancelled, 0);
    dprintf("ICAS: Reader claimed\n");
    return 0;
}
static uint32_t icas_release(struct icas_reader *self)
{
    InterlockedExchange(&self->cancelled, 1);
    self->mode = 0;
    self->claimed = false;
    return 0;
}
/* Claim precedes either open mode. The generic NFC mask uses bit 3 for
 * FeliCa; detection below returns that same value as the card type. */
static uint32_t icas_open(struct icas_reader *self, unsigned int mode, uint16_t types)
{
    if (!self->claimed || self->mode != 0) return ICAS_DEVICE_ERROR;
    self->mode = mode;
    self->card_types = types;
    InterlockedExchange(&self->cancelled, 0);
    return 0;
}
static uint32_t icas_open_felica(struct icas_reader *self)
{
    return icas_open(self, 1, 8);
}
static uint32_t icas_open_nfc(struct icas_reader *self, uint8_t device, uint16_t types)
{
    if (device != 0 || !(types & 9)) return ICAS_DEVICE_ERROR;
    return icas_open(self, 2, types);
}
static uint32_t icas_close(struct icas_reader *self)
{
    if (self->mode == 0) return ICAS_NOT_OPEN;
    self->mode = 0;
    return 0;
}
/* Both input paths feed the same eight-byte IDm. Poll at least once even for
 * a zero timeout, and check cancellation between polls. E_PENDING means the
 * shared reader is still starting and can be retried within this timeout. */
static uint32_t icas_wait_card(struct icas_reader *self, uint32_t timeout)
{
    ULONGLONG start = GetTickCount64();
    if (icas_cfg.icas_aimeio
            && !(icas_cfg.icas_aime_access_code ? (bool) icas_aimeio_access_poll : (bool) icas_aimeio_poll)
            && !icas_find_aimeio()) {
        dprintf("ICAS: AimeIO requires a rebuilt Segatools hook with the ICAS bridge\n");
        return ICAS_DEVICE_ERROR;
    }
    do {
        if (InterlockedCompareExchange(&self->cancelled, 0, 0)) return ICAS_CANCELLED;
        if (icas_cfg.icas_aimeio) {
            uint64_t idm = 0;
            HRESULT hr;
            if (icas_cfg.icas_aime_access_code) {
                uint8_t code[10] = {0};
                hr = icas_aimeio_access_poll(code);
                bool valid = hr == S_OK && icas_access_code_idm(code, self->idm);
                SecureZeroMemory(code, sizeof(code));
                if (hr == S_OK) return valid ? 0 : ICAS_DEVICE_ERROR;
            } else {
                hr = icas_aimeio_poll(&idm);
            }
            if (hr == S_OK && idm) {
                /* AimeIO returns a numeric IDm; TFPS expects wire byte order. */
                for (unsigned int i = 0; i < 8; i++) {
                    self->idm[i] = (uint8_t) (idm >> (56 - i * 8));
                }
                return 0;
            }
            if (FAILED(hr) && hr != E_PENDING) {
                dprintf("ICAS: AimeIO poll failed (%08x)\n", (unsigned int) hr);
                return ICAS_DEVICE_ERROR;
            }
        } else if (GetAsyncKeyState(icas_cfg.icas_scan) & 0x8000) {
            if (!icas_read_card(self->idm)) {
                dprintf("ICAS: Invalid or missing card file: %S\n", icas_cfg.icas_aime_access_code ? icas_cfg.icas_aime_path : icas_cfg.icas_card_path);
                return ICAS_DEVICE_ERROR;
            }
            return 0;
        }
        if (GetTickCount64() - start >= timeout) break;
        Sleep(10);
    } while (true);
    return ICAS_TIMEOUT;
}
static bool icas_write(struct icas_sink *sink, const uint8_t *bytes, size_t count)
{
    return sink && sink->vtable && sink->vtable->write
            && sink->vtable->write(sink, bytes, count);
}
/* Fixed polling parameters for the emulated FeliCa card. */
static const uint8_t icas_pmm[8] = {0x00, 0xf1, 0x00, 0x00, 0x00, 0x01, 0x43, 0x00};

/* Generic NFC detection returns IDm and PMm through separate sinks. Publish
 * the card type only after both outputs have been accepted by the client. */
static uint32_t icas_detect(struct icas_reader *self, uint32_t timeout,
        uint32_t *type, struct icas_sink *idm, struct icas_sink *pmm)
{
    uint32_t status;
    if (!type) return ICAS_INVALID;
    *type = 0;
    if (self->mode != 2) return ICAS_NOT_OPEN;
    if (!(self->card_types & 8)) return ICAS_DEVICE_ERROR;
    status = icas_wait_card(self, timeout);
    if (status) return status;
    if (!icas_write(idm, self->idm, 8) || !icas_write(pmm, icas_pmm, 8)) return ICAS_DEVICE_ERROR;
    *type = 8;
    dprintf("ICAS: Card tapped\n");
    return 0;
}

/* Handle the six-byte FeliCa Polling command (0x00). Its response contains
 * IDm and PMm, plus the requested system code when request code is 0x01.
 * Other card commands return a device error. */
static uint32_t icas_transact(struct icas_reader *self, uint32_t timeout,
        uint32_t retries, const uint8_t *req, size_t size, struct icas_sink *sink)
{
    uint8_t response[20];
    uint32_t status;
    size_t n;
    (void) retries;
    if (!self->mode) return ICAS_NOT_OPEN;
    if (!req || size != 6 || req[0] != 6 || req[1] != 0 || req[4] > 1) {
        dprintf("ICAS: Unsupported FeliCa transaction (size=%u)\n", (unsigned int) size);
        return ICAS_DEVICE_ERROR;
    }
    status = icas_wait_card(self, timeout);
    if (status) return status;
    n = req[4] == 1 ? 20 : 18;
    response[0] = (uint8_t) n;
    response[1] = 1;
    memcpy(response + 2, self->idm, 8);
    memcpy(response + 10, icas_pmm, 8);
    response[18] = req[2];
    response[19] = req[3];
    return icas_write(sink, response, n) ? 0 : ICAS_DEVICE_ERROR;
}
static uint32_t icas_reserved(struct icas_reader *self) { (void) self; return 0; }
static uint32_t icas_led(struct icas_reader *self, uint32_t device, uint32_t color,
        uint32_t pattern, const uint8_t *data, size_t size)
{
    (void) self; (void) device; (void) color; (void) pattern; (void) data; (void) size;
    return 0;
}
static void icas_cancel(struct icas_reader *self) { InterlockedExchange(&self->cancelled, 1); }
static void icas_reset_cancel(struct icas_reader *self) { InterlockedExchange(&self->cancelled, 0); }
static const char *icas_error(struct icas_reader *self) { (void) self; return "ICAS emulated reader"; }
static void icas_destroy(struct icas_reader *self) { free(self); }
static void *icas_delete(struct icas_reader *self, unsigned int flags)
{
    /* MSVC scalar deleting destructor: bit 0 requests storage release. */
    if (flags & 1) free(self);
    return self;
}
static const struct icas_vtable icas_vtable = {
    icas_configure, icas_claim, icas_release, icas_open_felica, icas_close,
    icas_transact, icas_open_nfc, icas_close, icas_detect, icas_reserved,
    icas_transact, icas_led, icas_cancel, icas_reset_cancel, icas_error,
    icas_destroy, icas_delete,
};
static struct icas_reader *WINAPI icas_create(uint32_t version)
{
    struct icas_reader *self;
    /* The low byte is the patch revision of the TFPS adapter interface. */
    if ((version & 0xffffff00u) != 0x01000300u) return NULL;
    self = calloc(1, sizeof(*self));
    if (self) self->vtable = &icas_vtable;
    return self;
}

/* amdaemon registers its built-in adapter directly, bypassing DLL lookup.
 * setRwAdapter transfers reader ownership to the real payment client on success. */
static uint32_t icas_set_rw_adapter(void *self, uint32_t version,
        struct icas_reader *original, const char *connection)
{
    struct icas_reader *reader = icas_create(version);
    uint32_t result;
    if (!reader) return next_set_rw_adapter(self, version, original, connection);
    result = next_set_rw_adapter(self, version, reader, connection);
    if (result == 0) {
        /* The host can retain/re-register its embedded adapter. Never destroy
         * that object here: it was not handed to the real client by this call. */
        dprintf("ICAS: Replaced directly registered reader\n");
    } else {
        icas_destroy(reader);
    }
    return result;
}

static uint32_t icas_payment_initialize(void *self, uint64_t arg)
{
    /* Platform startup applies its own GetProcAddress hooks after injection.
     * Restore our factory interception at the point where it will be used. */
    hook_table_apply(icas_payment, "kernel32.dll", icas_symbols, _countof(icas_symbols));
    return next_payment_initialize(self, arg);
}

static HRESULT icas_hook_registration(HMODULE payment)
{
    uint32_t (WINAPI *version)(void) = (void *) GetProcAddress(payment, "ThincaPaymentGetVersion");
    void *(WINAPI *instance)(uint32_t) = (void *) GetProcAddress(payment, "ThincaPaymentGetInstance");
    void ***object;
    void **slot;
    DWORD protection, ignored;
    /* Verified interface 1.4.11: vtable slot 8, setRwAdapter at RVA 4e200.
     * Refuse other builds instead of patching an unverified method. */
    if (!version || !instance || version() != 0x01040b00) return E_NOTIMPL;
    object = instance(version());
    if (!object || !*object) return E_FAIL;
    slot = &(*object)[8];
    if (*slot == (void *) icas_set_rw_adapter) return S_OK;
    if (*slot != (uint8_t *) payment + 0x4e200) return E_NOTIMPL;
    if (!VirtualProtect(*object, 9 * sizeof(void *), PAGE_READWRITE, &protection))
        return HRESULT_FROM_WIN32(GetLastError());
    icas_payment = payment;
    next_payment_initialize = (void *) (*object)[2];
    (*object)[2] = (void *) icas_payment_initialize;
    next_set_rw_adapter = (void *) *slot;
    *slot = (void *) icas_set_rw_adapter;
    VirtualProtect(*object, 9 * sizeof(void *), protection, &ignored);
    return S_OK;
}
static FARPROC WINAPI icas_GetProcAddress(HMODULE module, LPCSTR name)
{
    if ((module == icas_adapter || module == icas_adapter_debug) && module != NULL) {
        /* GetProcAddress accepts either a string or a low-word ordinal. */
        if ((uintptr_t) name == 2 || ((uintptr_t) name > 0xffff
                && strcmp(name, "CreateThincaRwAdapter") == 0)) {
            return (FARPROC) icas_create;
        }
    }
    return next_GetProcAddress(module, name);
}
HRESULT icasrw_hook_init(const struct epay_config *cfg, HMODULE payment)
{
#ifndef _WIN64
    (void) cfg; (void) payment;
    /* The shared ini is also used by the 32-bit game process. Payments run
     * in 64-bit amdaemon, so leave the game process alone. */
    return S_FALSE;
#else
    if (!payment || cfg->hook || (!cfg->icas_aimeio
            && (!cfg->icas_scan || cfg->icas_scan > 255))) return E_INVALIDARG;
    icas_cfg = *cfg;
    icas_adapter = GetModuleHandleW(L"aime_rw_adapterMD.dll");
    icas_adapter_debug = GetModuleHandleW(L"aime_rw_adapterMDD.dll");
    if (!icas_adapter && !icas_adapter_debug) return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    HRESULT hr = icas_hook_registration(payment);
    if (FAILED(hr)) return hr;
    hook_table_apply(payment, "kernel32.dll", icas_symbols, _countof(icas_symbols));
    if (!next_GetProcAddress) return E_FAIL;
    dprintf("ICAS: Emulation enabled (scan key=0x%02x)\n", cfg->icas_scan);
    return S_OK;
#endif
}

/* Reuse the normal reader files and scan key by default. Only the FeliCa
 * file has an ICAS-specific override, for a separate payment identity. */
void icasrw_config_load(struct epay_config *cfg, const wchar_t *filename)
{
    cfg->icas_aime_access_code = GetPrivateProfileIntW(L"icas", L"aimeAccessCode", 0, filename);
    GetPrivateProfileStringW(L"aime", L"aimePath", L"DEVICE\\aime.txt",
            cfg->icas_aime_path, _countof(cfg->icas_aime_path), filename);
    cfg->icas_aimeio = GetPrivateProfileIntW(L"icas", L"aimeio", 0, filename);
    cfg->icas_emulate = GetPrivateProfileIntW(L"icas", L"enable", 0, filename);
    GetPrivateProfileStringW(L"aime", L"felicaPath", L"DEVICE\\felica.txt",
            cfg->icas_card_path, _countof(cfg->icas_card_path), filename);
    wchar_t card_path[MAX_PATH] = {0};
    GetPrivateProfileStringW(L"icas", L"felicaPath", cfg->icas_card_path,
            card_path, _countof(card_path), filename);
    memcpy(cfg->icas_card_path, card_path, sizeof(card_path));
    cfg->icas_scan = GetPrivateProfileIntW(L"aime", L"scan", VK_RETURN, filename);
}
