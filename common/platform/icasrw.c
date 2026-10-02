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

/* Adapter statuses recovered from aime_rw_adapterMD 1.0.3 (ef266a2e). These
 * are not HRESULTs. Never return success without filling the output sinks. */
#define ICAS_INVALID 0x10001u
#define ICAS_NOT_OPEN 0x10002u
#define ICAS_MODE_CONFLICT 0x10003u
#define ICAS_DEVICE_ERROR 0x20001u
#define ICAS_TIMEOUT 0x20002u
#define ICAS_CANCELLED 0x20003u

#define ICAS_MODE_CLOSED 0
#define ICAS_MODE_FELICA 1
#define ICAS_MODE_NFC 2

/* Card-type bit for FeliCa in the open mask and the detected card type. */
#define ICAS_CARD_FELICA 0x0008u

#define ICAS_PAYMENT_VERSION 0x01040b00u
#define ICAS_ADAPTER_VERSION 0x01000300u

/* ThincaPayment 1.4.11 vtable slots and the setRwAdapter RVA they point to. */
#define ICAS_SLOT_INITIALIZE 2
#define ICAS_SLOT_SET_RW_ADAPTER 8
#define ICAS_SET_RW_ADAPTER_RVA 0x4e200

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
 * entries: the real payment client calls these methods through its vtable.
 * The client frees a reader through destroy; the real adapter's destroy
 * forwards to deleting_destructor(1). */
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
    unsigned int mode;
    uint16_t card_types;
    uint8_t idm[8];
};

static FARPROC WINAPI icas_GetProcAddress(HMODULE module, LPCSTR name);

static struct epay_config icas_cfg;
static HMODULE icas_payment;
static HMODULE icas_adapter;
static HMODULE icas_adapter_debug;
static HRESULT (*icas_aimeio_poll)(uint64_t *);
static HRESULT (*icas_aimeio_access_poll)(uint8_t *);
static FARPROC (WINAPI *next_GetProcAddress)(HMODULE, LPCSTR);
static uint32_t (*next_set_rw_adapter)(void *, uint32_t,
        struct icas_reader *, const char *);
static uint32_t (*next_payment_initialize)(void *, uint64_t);

/* Fixed polling parameters for the emulated FeliCa card, matching the
 * Amusement IC card emulated by the normal Aime reader. */
static const uint8_t icas_pmm[8] = {
    0x00, 0xf1, 0x00, 0x00, 0x00, 0x01, 0x43, 0x00,
};
static const uint8_t icas_system_code[2] = {0x88, 0xb4};

/* Resolve the shared reader bridge from the loaded game hook without
 * depending on its DLL name. The hook owns AimeIO initialization and
 * serializes reader access. */
static bool icas_find_aimeio(void)
{
    MODULEENTRY32W entry;
    HANDLE snapshot;
    bool access;

    access = icas_cfg.icas_aime_access_code;
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());

    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    memset(&entry, 0, sizeof(entry));
    entry.dwSize = sizeof(entry);

    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (access) {
                icas_aimeio_access_poll = (void *) GetProcAddress(
                        entry.hModule,
                        "segatools_icas_aimeio_poll_access_code");

                if (icas_aimeio_access_poll != NULL) {
                    break;
                }
            } else {
                icas_aimeio_poll = (void *) GetProcAddress(
                        entry.hModule,
                        "segatools_icas_aimeio_poll");

                if (icas_aimeio_poll != NULL) {
                    break;
                }
            }
        } while (Module32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);

    return access ? icas_aimeio_access_poll != NULL : icas_aimeio_poll != NULL;
}

/* AimeIO provides ten packed-BCD bytes. Hash exactly 20 ASCII digits, keeping
 * leading zeroes. Amusement IC codes contain a 3-digit company code, 12-digit
 * encoded card data, and 5-digit checksum. Keep the company namespace and all
 * card data; the checksum adds no independent entropy. Use all 64 output bits
 * without a fixed prefix. Both file and hardware inputs use this mapping. */
static bool icas_access_code_idm(const uint8_t code[10], uint8_t idm[8])
{
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    uint8_t digits[20];
    uint8_t digest[32];
    bool ok = false;
    unsigned int i;

    for (i = 0 ; i < 10 ; i++) {
        if ((code[i] >> 4) > 9 || (code[i] & 15) > 9) {
            return false;
        }

        digits[i * 2] = '0' + (code[i] >> 4);
        digits[i * 2 + 1] = '0' + (code[i] & 15);
    }

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0
            || BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0) < 0
            || BCryptHashData(hash, digits, sizeof(digits), 0) < 0
            || BCryptFinishHash(hash, digest, sizeof(digest), 0) < 0) {
        goto end;
    }

    memcpy(idm, digest, 8);
    ok = true;

end:
    if (hash != NULL) {
        BCryptDestroyHash(hash);
    }

    if (algorithm != NULL) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
    }

    SecureZeroMemory(digits, sizeof(digits));
    SecureZeroMemory(digest, sizeof(digest));

    return ok;
}

/* Read eight hex IDm bytes or ten packed-decimal access-code bytes.
 * Whitespace is allowed; malformed or trailing data is rejected. */
static bool icas_read_card(uint8_t idm[8])
{
    bool access = icas_cfg.icas_aime_access_code;
    unsigned int expected = access ? 10 : 8;
    unsigned int count = 0;
    uint8_t bytes[10];
    bool valid = true;
    int high = -1;
    int digit;
    int c;
    FILE *f;

    f = _wfopen(access ? icas_cfg.icas_aime_path : icas_cfg.icas_card_path, L"r");

    if (f == NULL) {
        return false;
    }

    while ((c = fgetc(f)) != EOF) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            continue;
        }

        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else {
            valid = false;
            break;
        }

        if ((access && digit > 9) || count >= expected) {
            valid = false;
            break;
        }

        if (high < 0) {
            high = digit;
        } else {
            bytes[count++] = (uint8_t) ((high << 4) | digit);
            high = -1;
        }
    }

    valid = valid && !ferror(f) && count == expected && high < 0;
    fclose(f);

    if (valid && access) {
        valid = icas_access_code_idm(bytes, idm);
    } else if (valid) {
        memcpy(idm, bytes, 8);
    }

    SecureZeroMemory(bytes, sizeof(bytes));

    return valid;
}

static uint32_t icas_configure(struct icas_reader *self, const char *connection)
{
    return connection != NULL ? 0 : ICAS_INVALID;
}

static uint32_t icas_claim(struct icas_reader *self)
{
    self->claimed = true;
    InterlockedExchange(&self->cancelled, 0);
    dprintf("ICAS: Reader claimed\n");

    return 0;
}

/* Release can run while another thread waits in detect. Setting the cancel
 * flag ends that wait; claim and open clear it for the next session. */
static uint32_t icas_release(struct icas_reader *self)
{
    InterlockedExchange(&self->cancelled, 1);
    self->mode = ICAS_MODE_CLOSED;
    self->claimed = false;

    return 0;
}

/* Claim precedes either open mode. Like the real adapter, reopening the
 * current mode is a device error and opening the other mode is a conflict. */
static uint32_t icas_open(struct icas_reader *self, unsigned int mode, uint16_t types)
{
    if (!self->claimed || self->mode == mode) {
        return ICAS_DEVICE_ERROR;
    }

    if (self->mode != ICAS_MODE_CLOSED) {
        return ICAS_MODE_CONFLICT;
    }

    self->mode = mode;
    self->card_types = types;
    InterlockedExchange(&self->cancelled, 0);

    return 0;
}

static uint32_t icas_open_felica(struct icas_reader *self)
{
    return icas_open(self, ICAS_MODE_FELICA, ICAS_CARD_FELICA);
}

/* The client opens reader 0 with card-type mask 0x0009. The real adapter
 * accepts any mask containing bit 0 or the FeliCa bit. */
static uint32_t icas_open_nfc(struct icas_reader *self, uint8_t device, uint16_t types)
{
    if (device != 0 || !(types & 0x0009)) {
        return ICAS_DEVICE_ERROR;
    }

    return icas_open(self, ICAS_MODE_NFC, types);
}

static uint32_t icas_close(struct icas_reader *self)
{
    if (self->mode == ICAS_MODE_CLOSED) {
        return ICAS_NOT_OPEN;
    }

    self->mode = ICAS_MODE_CLOSED;

    return 0;
}

static bool icas_aimeio_ready(void)
{
    if (icas_cfg.icas_aime_access_code) {
        return icas_aimeio_access_poll != NULL || icas_find_aimeio();
    } else {
        return icas_aimeio_poll != NULL || icas_find_aimeio();
    }
}

/* Poll the shared reader once. Returns S_OK with self->idm filled, S_FALSE
 * when no card is present, E_PENDING while the reader starts, or an error. */
static HRESULT icas_poll_aimeio(struct icas_reader *self)
{
    uint8_t code[10];
    uint64_t idm;
    HRESULT hr;
    bool valid;
    unsigned int i;

    if (icas_cfg.icas_aime_access_code) {
        memset(code, 0, sizeof(code));
        hr = icas_aimeio_access_poll(code);
        valid = hr == S_OK && icas_access_code_idm(code, self->idm);
        SecureZeroMemory(code, sizeof(code));

        if (hr == S_OK && !valid) {
            return E_INVALIDARG;
        }

        return hr;
    }

    idm = 0;
    hr = icas_aimeio_poll(&idm);

    if (hr != S_OK) {
        return hr;
    }

    if (idm == 0) {
        return S_FALSE;
    }

    /* AimeIO returns a numeric IDm; TFPS expects wire byte order. */
    for (i = 0 ; i < 8 ; i++) {
        self->idm[i] = (uint8_t) (idm >> (56 - i * 8));
    }

    return S_OK;
}

/* Both input paths feed the same eight-byte IDm. Poll at least once even for
 * a zero timeout, and check cancellation between polls. E_PENDING means the
 * shared reader is still starting and can be retried within this timeout. */
static uint32_t icas_wait_card(struct icas_reader *self, uint32_t timeout)
{
    ULONGLONG start;
    HRESULT hr;

    start = GetTickCount64();

    if (icas_cfg.icas_aimeio && !icas_aimeio_ready()) {
        dprintf("ICAS: AimeIO requires a Segatools hook with the ICAS bridge\n");

        return ICAS_DEVICE_ERROR;
    }

    for (;;) {
        if (InterlockedCompareExchange(&self->cancelled, 0, 0)) {
            return ICAS_CANCELLED;
        }

        if (icas_cfg.icas_aimeio) {
            hr = icas_poll_aimeio(self);

            if (hr == S_OK) {
                return 0;
            }

            if (FAILED(hr) && hr != E_PENDING) {
                dprintf("ICAS: AimeIO poll failed (%08x)\n", (unsigned int) hr);

                return ICAS_DEVICE_ERROR;
            }
        } else if (GetAsyncKeyState(icas_cfg.icas_scan) & 0x8000) {
            if (!icas_read_card(self->idm)) {
                dprintf("ICAS: Invalid or missing card file: %S\n",
                        icas_cfg.icas_aime_access_code
                                ? icas_cfg.icas_aime_path
                                : icas_cfg.icas_card_path);

                return ICAS_DEVICE_ERROR;
            }

            return 0;
        }

        if (GetTickCount64() - start >= timeout) {
            return ICAS_TIMEOUT;
        }

        Sleep(10);
    }
}

static bool icas_write(struct icas_sink *sink, const uint8_t *bytes, size_t count)
{
    return sink != NULL
            && sink->vtable != NULL
            && sink->vtable->write != NULL
            && sink->vtable->write(sink, bytes, count);
}

/* Generic NFC detection returns IDm and PMm through separate sinks. Publish
 * the card type only after both outputs have been accepted by the client. */
static uint32_t icas_detect(
        struct icas_reader *self,
        uint32_t timeout,
        uint32_t *type,
        struct icas_sink *idm,
        struct icas_sink *pmm)
{
    uint32_t status;

    if (type == NULL) {
        return ICAS_INVALID;
    }

    *type = 0;

    if (self->mode != ICAS_MODE_NFC) {
        return ICAS_NOT_OPEN;
    }

    if (!(self->card_types & ICAS_CARD_FELICA)) {
        return ICAS_DEVICE_ERROR;
    }

    status = icas_wait_card(self, timeout);

    if (status != 0) {
        return status;
    }

    if (!icas_write(idm, self->idm, 8) || !icas_write(pmm, icas_pmm, 8)) {
        return ICAS_DEVICE_ERROR;
    }

    *type = ICAS_CARD_FELICA;
    dprintf("ICAS: Card tapped\n");

    return 0;
}

/* Handle the six-byte FeliCa Polling command (0x00). Its response contains
 * IDm and PMm, plus a system code when the request code is 0x01. A wildcard
 * request (FFFF) reports the card's own system code, as a real card does.
 * Other card commands return a device error. */
static uint32_t icas_transact(
        struct icas_reader *self,
        uint32_t timeout,
        uint32_t retries,
        const uint8_t *req,
        size_t size,
        struct icas_sink *sink)
{
    uint8_t response[20];
    uint32_t status;
    size_t n;

    if (self->mode == ICAS_MODE_CLOSED) {
        return ICAS_NOT_OPEN;
    }

    if (req == NULL || size != 6 || req[0] != 6 || req[1] != 0 || req[4] > 1) {
        dprintf("ICAS: Unsupported FeliCa command (size=%u)\n", (unsigned int) size);

        return ICAS_DEVICE_ERROR;
    }

    status = icas_wait_card(self, timeout);

    if (status != 0) {
        return status;
    }

    /* Length byte (counting itself), response code 01, IDm, PMm, and the
     * optional system code. */
    n = req[4] == 1 ? 20 : 18;
    response[0] = (uint8_t) n;
    response[1] = 0x01;
    memcpy(response + 2, self->idm, 8);
    memcpy(response + 10, icas_pmm, 8);

    if (req[2] == 0xff && req[3] == 0xff) {
        memcpy(response + 18, icas_system_code, 2);
    } else {
        response[18] = req[2];
        response[19] = req[3];
    }

    return icas_write(sink, response, n) ? 0 : ICAS_DEVICE_ERROR;
}

static uint32_t icas_reserved(struct icas_reader *self)
{
    return 0;
}

static uint32_t icas_led(
        struct icas_reader *self,
        uint32_t device,
        uint32_t color,
        uint32_t pattern,
        const uint8_t *data,
        size_t size)
{
    return 0;
}

static void icas_cancel(struct icas_reader *self)
{
    InterlockedExchange(&self->cancelled, 1);
}

static void icas_reset_cancel(struct icas_reader *self)
{
    InterlockedExchange(&self->cancelled, 0);
}

static const char *icas_error(struct icas_reader *self)
{
    return "ICAS emulated reader";
}

static void icas_destroy(struct icas_reader *self)
{
    free(self);
}

/* MSVC scalar deleting destructor: bit 0 requests storage release. */
static void *icas_delete(struct icas_reader *self, unsigned int flags)
{
    if (flags & 1) {
        free(self);
    }

    return self;
}

static const struct icas_vtable icas_vtable = {
    .configure = icas_configure,
    .claim = icas_claim,
    .release = icas_release,
    .open_felica = icas_open_felica,
    .close_felica = icas_close,
    .transact = icas_transact,
    .open_nfc = icas_open_nfc,
    .close_nfc = icas_close,
    .detect = icas_detect,
    .reserved = icas_reserved,
    .transact_retry = icas_transact,
    .led = icas_led,
    .cancel = icas_cancel,
    .reset_cancel = icas_reset_cancel,
    .error = icas_error,
    .destroy = icas_destroy,
    .deleting_destructor = icas_delete,
};

/* The low byte of the version is the adapter interface patch revision. */
static struct icas_reader *WINAPI icas_create(uint32_t version)
{
    struct icas_reader *self;

    if ((version & 0xffffff00u) != ICAS_ADAPTER_VERSION) {
        return NULL;
    }

    self = calloc(1, sizeof(*self));

    if (self != NULL) {
        self->vtable = &icas_vtable;
    }

    return self;
}

/* amdaemon can register its built-in adapter directly, bypassing the factory.
 * On success the payment client owns the emulated reader. The original stays
 * with amdaemon, which can retain or re-register it, so never destroy it.
 * Registration fails (0xCE) until the client is initialized; free our reader
 * then, because the client never took it. */
static uint32_t icas_set_rw_adapter(
        void *self,
        uint32_t version,
        struct icas_reader *original,
        const char *connection)
{
    struct icas_reader *reader;
    uint32_t result;

    reader = icas_create(version);

    if (reader == NULL) {
        return next_set_rw_adapter(self, version, original, connection);
    }

    result = next_set_rw_adapter(self, version, reader, connection);

    if (result == 0) {
        dprintf("ICAS: Replaced directly registered reader\n");
    } else {
        icas_destroy(reader);
    }

    return result;
}

/* Patch the client's GetProcAddress import. Platform hooks installed after
 * ours (capnhook's proc_addr) overwrite the same import. Chain to whatever
 * hook is there instead of bypassing it; those hooks link to the real
 * GetProcAddress during earlier platform initialization. */
static void icas_hook_lookup(void)
{
    FARPROC (WINAPI *current)(HMODULE, LPCSTR) = NULL;
    struct hook_symbol sym = {
        .name = "GetProcAddress",
        .patch = icas_GetProcAddress,
        .link = (void **) &current,
    };

    hook_table_apply(icas_payment, "kernel32.dll", &sym, 1);

    if (current != NULL && current != icas_GetProcAddress) {
        next_GetProcAddress = current;
    }
}

/* Restore the factory hook immediately before the client looks up its
 * reader adapter. */
static uint32_t icas_payment_initialize(void *self, uint64_t arg)
{
    icas_hook_lookup();

    return next_payment_initialize(self, arg);
}

static HRESULT icas_hook_registration(HMODULE payment)
{
    uint32_t (WINAPI *get_version)(void);
    void *(WINAPI *get_instance)(uint32_t);
    DWORD protection;
    DWORD ignored;
    uint32_t version;
    void ***object;
    void **vtable;

    get_version = (void *) GetProcAddress(payment, "ThincaPaymentGetVersion");
    get_instance = (void *) GetProcAddress(payment, "ThincaPaymentGetInstance");

    if (get_version == NULL || get_instance == NULL) {
        dprintf("ICAS: ThincaPayment.dll has no payment client exports\n");

        return E_NOTIMPL;
    }

    /* Refuse other builds instead of patching an unverified method. */
    version = get_version();

    if (version != ICAS_PAYMENT_VERSION) {
        dprintf("ICAS: Unsupported ThincaPayment version %08x, need %08x\n",
                version,
                ICAS_PAYMENT_VERSION);

        return E_NOTIMPL;
    }

    /* GetInstance only returns the DLL's global client object, so calling it
     * early has no side effects. amdaemon gets the same object later, so
     * patching its vtable in place covers every call the daemon makes. */
    object = get_instance(version);

    if (object == NULL || *object == NULL) {
        dprintf("ICAS: ThincaPayment returned no client instance\n");

        return E_FAIL;
    }

    vtable = *object;

    if (vtable[ICAS_SLOT_SET_RW_ADAPTER] == (void *) icas_set_rw_adapter) {
        return S_OK;
    }

    if (vtable[ICAS_SLOT_SET_RW_ADAPTER] != (uint8_t *) payment + ICAS_SET_RW_ADAPTER_RVA) {
        dprintf("ICAS: Unexpected ThincaPayment setRwAdapter entry\n");

        return E_NOTIMPL;
    }

    /* The vtable lives in read-only data; slots 0 to 8 cover both patches. */
    if (!VirtualProtect(
            vtable,
            (ICAS_SLOT_SET_RW_ADAPTER + 1) * sizeof(void *),
            PAGE_READWRITE,
            &protection)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    next_payment_initialize = (void *) vtable[ICAS_SLOT_INITIALIZE];
    vtable[ICAS_SLOT_INITIALIZE] = (void *) icas_payment_initialize;
    next_set_rw_adapter = (void *) vtable[ICAS_SLOT_SET_RW_ADAPTER];
    vtable[ICAS_SLOT_SET_RW_ADAPTER] = (void *) icas_set_rw_adapter;

    VirtualProtect(
            vtable,
            (ICAS_SLOT_SET_RW_ADAPTER + 1) * sizeof(void *),
            protection,
            &ignored);

    return S_OK;
}

static FARPROC WINAPI icas_GetProcAddress(HMODULE module, LPCSTR name)
{
    uintptr_t ordinal = (uintptr_t) name;

    if (module != NULL && (module == icas_adapter || module == icas_adapter_debug)) {
        /* GetProcAddress accepts either a string or a low-word ordinal. */
        if (ordinal == 2
                || (ordinal > 0xffff && strcmp(name, "CreateThincaRwAdapter") == 0)) {
            return (FARPROC) icas_create;
        }
    }

    return next_GetProcAddress(module, name);
}

HRESULT icasrw_hook_init(const struct epay_config *cfg, HMODULE payment)
{
#ifndef _WIN64
    /* The shared ini is also used by 32-bit game processes. Payments run in
     * 64-bit amdaemon, so leave those processes alone. A 64-bit game process
     * installs the same hooks, which stay unused. */
    return S_FALSE;
#else
    HRESULT hr;

    if (payment == NULL) {
        dprintf("ICAS: ThincaPayment.dll is not loaded\n");

        return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    }

    if (cfg->hook) {
        dprintf("ICAS: [epay] hook=1 replaces ThincaPayment; set hook=0\n");

        return E_INVALIDARG;
    }

    if (!cfg->icas_aimeio && (cfg->icas_scan == 0 || cfg->icas_scan > 255)) {
        dprintf("ICAS: Invalid [aime] scan key %u\n", cfg->icas_scan);

        return E_INVALIDARG;
    }

    /* epay preloads the release and debug adapters. Match lookups by module
     * handle rather than file name, so file name case does not matter. */
    icas_cfg = *cfg;
    icas_adapter = GetModuleHandleW(L"aime_rw_adapterMD.dll");
    icas_adapter_debug = GetModuleHandleW(L"aime_rw_adapterMDD.dll");

    if (icas_adapter == NULL && icas_adapter_debug == NULL) {
        dprintf("ICAS: aime_rw_adapterMD.dll is not loaded\n");

        return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    }

    hr = icas_hook_registration(payment);

    if (FAILED(hr)) {
        return hr;
    }

    icas_payment = payment;
    icas_hook_lookup();

    if (next_GetProcAddress == NULL) {
        dprintf("ICAS: ThincaPayment.dll does not import GetProcAddress\n");

        return E_FAIL;
    }

    if (cfg->icas_aimeio) {
        dprintf("ICAS: Emulation enabled (AimeIO %s)\n",
                cfg->icas_aime_access_code ? "access code" : "FeliCa IDm");
    } else {
        dprintf("ICAS: Emulation enabled (scan key=0x%02x, file=%S)\n",
                cfg->icas_scan,
                cfg->icas_aime_access_code ? cfg->icas_aime_path : cfg->icas_card_path);
    }

    return S_OK;
#endif
}

/* Reuse the normal reader files and scan key by default. Only the FeliCa
 * file has an ICAS-specific override, for a separate payment identity. */
void icasrw_config_load(struct epay_config *cfg, const wchar_t *filename)
{
    wchar_t card_path[MAX_PATH];

    cfg->icas_emulate = GetPrivateProfileIntW(L"icas", L"enable", 0, filename);
    cfg->icas_aimeio = GetPrivateProfileIntW(L"icas", L"aimeio", 0, filename);
    cfg->icas_aime_access_code = GetPrivateProfileIntW(
            L"icas",
            L"aimeAccessCode",
            0,
            filename);

    GetPrivateProfileStringW(
            L"aime",
            L"aimePath",
            L"DEVICE\\aime.txt",
            cfg->icas_aime_path,
            _countof(cfg->icas_aime_path),
            filename);

    GetPrivateProfileStringW(
            L"aime",
            L"felicaPath",
            L"DEVICE\\felica.txt",
            cfg->icas_card_path,
            _countof(cfg->icas_card_path),
            filename);

    GetPrivateProfileStringW(
            L"icas",
            L"felicaPath",
            cfg->icas_card_path,
            card_path,
            _countof(card_path),
            filename);

    memcpy(cfg->icas_card_path, card_path, sizeof(card_path));

    cfg->icas_scan = GetPrivateProfileIntW(L"aime", L"scan", VK_RETURN, filename);
}
