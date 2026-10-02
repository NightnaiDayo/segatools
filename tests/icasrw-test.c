/* Standalone Windows x64 adapter contract tests. Build with capnhook headers:
 * zig cc -target x86_64-windows-gnu -DNDEBUG -Icommon -I<capnhook>
 *     tests/icasrw-test.c -lbcrypt -o icasrw-test.exe
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ULONGLONG test_clock;
static SHORT test_key;
static SHORT test_key_state(int key) { (void) key; return test_key; }
static ULONGLONG test_ticks(void) { return test_clock; }
static void test_sleep(DWORD ms) { test_clock += ms; }
#define GetAsyncKeyState test_key_state
#define GetTickCount64 test_ticks
#define Sleep test_sleep
#include "platform/icasrw.c"

void hook_table_apply(HMODULE target, const char *depname,
        const struct hook_symbol *syms, size_t nsyms)
{
    (void) target; (void) depname; (void) syms; (void) nsyms;
}
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "Failed line %d: %s\n", __LINE__, #expr); exit(1); } } while (0)
static struct icas_reader *registered_reader;
static unsigned int original_destroyed;
static void original_destroy(struct icas_reader *reader)
{
    original_destroyed++;
    free(reader);
}
static uint32_t register_reader(void *self, uint32_t version,
        struct icas_reader *reader, const char *connection)
{
    (void) self;
    CHECK(version == 0x01000300);
    CHECK(connection && strcmp(connection, "test") == 0);
    CHECK(reader->vtable == &icas_vtable);
    registered_reader = reader;
    return 0;
}
struct test_sink {
    const struct icas_sink_vtable *vtable;
    uint8_t bytes[32];
    size_t count;
    bool fail;
};
static bool test_write(struct icas_sink *base, const uint8_t *bytes, size_t n)
{
    struct test_sink *sink = (struct test_sink *) base;
    if (sink->fail || n > sizeof(sink->bytes)) return false;
    memcpy(sink->bytes, bytes, n);
    sink->count = n;
    return true;
}
static const struct icas_sink_vtable sink_vtable = {NULL, test_write};
static void card_file(const char *text)
{
    FILE *f = _wfopen(icas_cfg.icas_card_path, L"w");
    CHECK(f != NULL);
    CHECK(fputs(text, f) >= 0);
    CHECK(fclose(f) == 0);
}
static HRESULT provider_result;
static HRESULT test_provider(uint64_t *idm)
{
    *idm = 0x036f9e9c732c34b6ULL;
    return provider_result;
}

static uint8_t test_access_code[10] = {0x00, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78};
static HRESULT test_access_provider(uint8_t *code)
{
    memcpy(code, test_access_code, 10);
    return provider_result;
}

int main(void)
{
    wchar_t temp[MAX_PATH];
    CHECK(GetTempPathW(MAX_PATH, temp));
    CHECK(GetTempFileNameW(temp, L"ica", 0, icas_cfg.icas_card_path));
    icas_cfg.icas_scan = VK_RETURN;
    struct icas_reader *reader = icas_create(0x01000300);
    struct test_sink idm = {&sink_vtable, {0}, 0, false};
    struct test_sink pmm = {&sink_vtable, {0}, 0, false};
    uint32_t type = 123;
    CHECK(reader);
    CHECK(!icas_create(0x02000000));
    CHECK(icas_open_nfc(reader, 0, 9) == ICAS_DEVICE_ERROR);
    CHECK(icas_claim(reader) == 0);
    CHECK(icas_open_nfc(reader, 0, 9) == 0);
    /* Match the real adapter: reopen is a device error, other mode conflicts. */
    CHECK(icas_open_nfc(reader, 0, 9) == ICAS_DEVICE_ERROR);
    CHECK(icas_open_felica(reader) == ICAS_MODE_CONFLICT);
    CHECK(icas_detect(reader, 30, &type, (void *) &idm, (void *) &pmm) == ICAS_TIMEOUT);
    CHECK(type == 0 && idm.count == 0 && test_clock == 30);
    icas_cancel(reader);
    CHECK(icas_detect(reader, 100, &type, (void *) &idm, (void *) &pmm) == ICAS_CANCELLED);
    icas_reset_cancel(reader);
    test_key = (SHORT) 0x8000;
    card_file("01 23 45 67 89 AB CD EF\n");
    CHECK(icas_detect(reader, 100, &type, (void *) &idm, (void *) &pmm) == 0);
    CHECK(type == 8 && idm.count == 8 && pmm.count == 8);
    CHECK(idm.bytes[0] == 1 && idm.bytes[7] == 0xef);
    CHECK(memcmp(pmm.bytes, icas_pmm, 8) == 0);
    idm.fail = true;
    CHECK(icas_detect(reader, 100, &type, (void *) &idm, (void *) &pmm) == ICAS_DEVICE_ERROR);
    CHECK(type == 0);
    idm.fail = false;
    card_file("0123456789ABCDEF00");
    CHECK(icas_detect(reader, 100, &type, (void *) &idm, (void *) &pmm) == ICAS_DEVICE_ERROR);
    card_file("0123456789ABCDEZ");
    CHECK(!icas_read_card(reader->idm));
    card_file("0123456789ABCDE");
    CHECK(!icas_read_card(reader->idm));
    card_file("0123456789ABCDEF");
    uint8_t poll[] = {6, 0, 0xff, 0xff, 1, 0};
    CHECK(icas_transact(reader, 10, 0, poll, sizeof(poll), (void *) &idm) == 0);
    CHECK(idm.count == 20 && idm.bytes[0] == 20 && idm.bytes[1] == 1);
    CHECK(idm.bytes[18] == 0x88 && idm.bytes[19] == 0xb4);
    poll[2] = 0xfe;
    poll[3] = 0x00;
    CHECK(icas_transact(reader, 10, 0, poll, sizeof(poll), (void *) &idm) == 0);
    CHECK(idm.bytes[18] == 0xfe && idm.bytes[19] == 0x00);
    poll[4] = 0;
    CHECK(icas_transact(reader, 10, 0, poll, sizeof(poll), (void *) &idm) == 0);
    CHECK(idm.count == 18);
    poll[1] = 6;
    CHECK(icas_transact(reader, 10, 0, poll, sizeof(poll), (void *) &idm) == ICAS_DEVICE_ERROR);
    icas_cfg.icas_aimeio = true;
    icas_aimeio_poll = test_provider;
    test_key = 0;
    provider_result = S_OK;
    CHECK(icas_detect(reader, 30, &type, (void *) &idm, (void *) &pmm) == 0);
    const uint8_t expected[] = {3, 0x6f, 0x9e, 0x9c, 0x73, 0x2c, 0x34, 0xb6};
    CHECK(memcmp(idm.bytes, expected, 8) == 0);
    provider_result = S_FALSE;
    test_key = (SHORT) 0x8000; /* Hardware mode must never fall back to the file. */
    CHECK(icas_detect(reader, 30, &type, (void *) &idm, (void *) &pmm) == ICAS_TIMEOUT);
    provider_result = E_PENDING;
    CHECK(icas_detect(reader, 30, &type, (void *) &idm, (void *) &pmm) == ICAS_TIMEOUT);
    provider_result = E_FAIL;
    CHECK(icas_detect(reader, 30, &type, (void *) &idm, (void *) &pmm) == ICAS_DEVICE_ERROR);
    icas_cancel(reader);
    provider_result = S_OK;
    CHECK(icas_detect(reader, 30, &type, (void *) &idm, (void *) &pmm) == ICAS_CANCELLED);
    icas_reset_cancel(reader);
    icas_cfg.icas_aimeio = false;
    icas_cfg.icas_aime_access_code = true;
    wcscpy(icas_cfg.icas_aime_path, icas_cfg.icas_card_path);
    card_file("00 123456789012345678\n");
    test_key = (SHORT) 0x8000;
    const uint8_t mapped[] = {0x3d, 0xea, 0x87, 0x3e, 0x45, 0xc0, 0x5c, 0x48};
    CHECK(icas_detect(reader, 30, &type, (void *) &idm, (void *) &pmm) == 0);
    CHECK(memcmp(idm.bytes, mapped, 8) == 0);
    card_file("0012345678901234567");
    CHECK(!icas_read_card(reader->idm));
    card_file("001234567890123456789");
    CHECK(!icas_read_card(reader->idm));
    card_file("0012345678901234567a");
    CHECK(!icas_read_card(reader->idm));
    icas_cfg.icas_aimeio = true;
    icas_aimeio_access_poll = test_access_provider;
    test_key = 0;
    provider_result = S_OK;
    CHECK(icas_detect(reader, 30, &type, (void *) &idm, (void *) &pmm) == 0);
    CHECK(memcmp(idm.bytes, mapped, 8) == 0);
    test_access_code[0] = 0xa0;
    CHECK(icas_detect(reader, 30, &type, (void *) &idm, (void *) &pmm) == ICAS_DEVICE_ERROR);
    test_access_code[0] = 0;
    provider_result = S_FALSE;
    CHECK(icas_detect(reader, 30, &type, (void *) &idm, (void *) &pmm) == ICAS_TIMEOUT);
    provider_result = E_FAIL;
    CHECK(icas_detect(reader, 30, &type, (void *) &idm, (void *) &pmm) == ICAS_DEVICE_ERROR);
    icas_cfg.icas_aime_access_code = false;
    icas_cfg.icas_aimeio = false;
    CHECK(icas_close(reader) == 0);
    CHECK(icas_detect(reader, 0, &type, (void *) &idm, (void *) &pmm) == ICAS_NOT_OPEN);
    CHECK(icas_release(reader) == 0);
    icas_destroy(reader);
    CHECK(DeleteFileW(icas_cfg.icas_card_path));
    struct icas_vtable original_table = icas_vtable;
    original_table.destroy = original_destroy;
    struct icas_reader *original = icas_create(0x01000300);
    original->vtable = &original_table;
    next_set_rw_adapter = register_reader;
    CHECK(icas_set_rw_adapter(NULL, 0x01000300, original, "test") == 0);
    CHECK(original_destroyed == 0 && registered_reader != original);
    original->vtable->destroy(original);
    icas_destroy(registered_reader);
    wchar_t ini[MAX_PATH];
    struct epay_config config = {0};
    config.hook = true;
    config.icas_scan = VK_RETURN;
    CHECK(icasrw_hook_init(&config, (HMODULE) 1) == E_INVALIDARG);
    CHECK(icasrw_hook_init(&config, NULL) == HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND));
    config.hook = false;
    config.icas_scan = 0;
    CHECK(icasrw_hook_init(&config, (HMODULE) 1) == E_INVALIDARG);
    memset(&config, 0, sizeof(config));
    CHECK(GetTempFileNameW(temp, L"cfg", 0, ini));
    icasrw_config_load(&config, ini);
    CHECK(!config.icas_emulate);
    CHECK(!config.icas_aimeio);
    CHECK(!config.icas_aime_access_code);
    CHECK(wcscmp(config.icas_card_path, L"DEVICE\\felica.txt") == 0);
    CHECK(WritePrivateProfileStringW(L"aime", L"felicaPath", L"DEVICE\\fallback.txt", ini));
    icasrw_config_load(&config, ini);
    CHECK(wcscmp(config.icas_card_path, L"DEVICE\\fallback.txt") == 0);
    CHECK(WritePrivateProfileStringW(L"icas", L"enable", L"1", ini));
    CHECK(WritePrivateProfileStringW(L"icas", L"aimeio", L"1", ini));
    CHECK(WritePrivateProfileStringW(L"icas", L"felicaPath", L"DEVICE\\payment.txt", ini));
    CHECK(wcscmp(config.icas_aime_path, L"DEVICE\\aime.txt") == 0);
    CHECK(WritePrivateProfileStringW(L"icas", L"aimeAccessCode", L"1", ini));
    CHECK(WritePrivateProfileStringW(L"aime", L"aimePath", L"DEVICE\\custom-aime.txt", ini));
    icasrw_config_load(&config, ini);
    CHECK(config.icas_emulate);
    CHECK(config.icas_aimeio);
    CHECK(config.icas_aime_access_code);
    CHECK(wcscmp(config.icas_card_path, L"DEVICE\\payment.txt") == 0);
    CHECK(wcscmp(config.icas_aime_path, L"DEVICE\\custom-aime.txt") == 0);
    CHECK(DeleteFileW(ini));
    puts("ICAS adapter contract tests passed");
    return 0;
}
