/* Smoke test against the user's real DLLs; no payment/network request is made.
 * Build with icasrw.c included below plus capnhook hook/{table,pe,peb}.c
 * and link bcrypt (-lbcrypt with Zig/MinGW).
 * Run with the game bin directory as cwd so the client DLLs can be loaded.
 */
#include "platform/icasrw.c"
static uint32_t test_initialize(void *self, uint64_t arg)
{
    (void) self; (void) arg;
    return 42;
}

int main(void)
{
    HMODULE adapter = LoadLibraryW(L"aime_rw_adapterMD.dll");
    HMODULE payment = LoadLibraryW(L"ThincaPayment.dll");
    struct epay_config cfg = {0};
    if (!adapter || !payment) {
        fprintf(stderr, "Cannot load client DLLs: %lu\n", GetLastError());
        return 1;
    }
    uint32_t (*version)(void) = (void *) GetProcAddress(adapter, "GetThincaRwAdapterVersion");
    if (!version || (version() & 0xffffff00u) != 0x01000300u) return 2;
    cfg.icas_scan = VK_RETURN;
    if (FAILED(icasrw_hook_init(&cfg, payment))) return 3;
    uint32_t (WINAPI *payment_version)(void) = (void *) GetProcAddress(payment, "ThincaPaymentGetVersion");
    void *(WINAPI *payment_instance)(uint32_t) = (void *) GetProcAddress(payment, "ThincaPaymentGetInstance");
    void ***client = payment_instance(payment_version());
    if (!client || (*client)[8] != (void *) icas_set_rw_adapter) return 11;
    if ((void *) next_set_rw_adapter != (uint8_t *) payment + 0x4e200) return 12;
    /* Verify the actual client's import, rather than call our hook directly. */
    uint8_t *base = (void *) payment;
    IMAGE_DOS_HEADER *dos = (void *) base;
    IMAGE_NT_HEADERS *nt = (void *) (base + dos->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *imp = (void *) (base + nt->OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    FARPROC (WINAPI *lookup)(HMODULE, LPCSTR) = NULL;
    for (; imp->Name; imp++) {
        IMAGE_THUNK_DATA *thunk = (void *) (base + imp->FirstThunk);
        for (; thunk->u1.Function; thunk++) {
            if (thunk->u1.Function == (uintptr_t) icas_GetProcAddress) lookup = (void *) thunk->u1.Function;
        }
    }
    if (!lookup) return 4;
    struct hook_symbol overwritten = {.name = "GetProcAddress", .patch = GetProcAddress};
    hook_table_apply(payment, "kernel32.dll", &overwritten, 1);
    next_payment_initialize = test_initialize;
    if (icas_payment_initialize(NULL, 0) != 42) return 13;
    bool restored = false;
    imp = (void *) (base + nt->OptionalHeader
            .DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; imp->Name; imp++) {
        IMAGE_THUNK_DATA *thunk = (void *) (base + imp->FirstThunk);
        for (; thunk->u1.Function; thunk++)
            if (thunk->u1.Function == (uintptr_t) icas_GetProcAddress) restored = true;
    }
    if (!restored) return 14;
    struct icas_reader *(*factory)(uint32_t) = (void *) lookup(adapter, "CreateThincaRwAdapter");
    if (!factory || (void *) factory != (void *) lookup(adapter, (LPCSTR) 2)) return 5;
    if (lookup(adapter, "GetThincaRwAdapterVersion") != (FARPROC) version) return 6;
    struct icas_reader *reader = factory(version());
    if (!reader || reader->vtable != &icas_vtable) return 7;
    if (reader->vtable->configure(reader, "\\\\.\\COM4:115200:00")) return 8;
    if (reader->vtable->claim(reader) || reader->vtable->open_nfc(reader, 0, 9)) return 9;
    if (reader->vtable->close_nfc(reader) || reader->vtable->release(reader)) return 10;
    reader->vtable->destroy(reader);
    puts("Real ThincaPayment factory interception passed (no payment attempted)");
    return 0;
}
