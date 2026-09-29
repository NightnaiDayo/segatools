/* chusanApp runs a set of environment and anti-tamper checks before the
   first input is processed, and terminates the process if any of them
   fail:

     1. "USERNAME=AppUser" must be present in the process environment.
     2. PEB+0xF00 must equal ror(GetCurrentProcessId(), 11).
     3. RTL_USER_PROCESS_PARAMETERS.Flags bit 0x4000 must be set.
     4. The process must appear undebugged.
     5. The kernel module list must contain four SEGA driver images.
     6. The running image must pass an Authenticode check.

   Older chusan builds do not perform these checks at all. Nothing here
   depends on a game build address or on a particular game version, so
   those builds simply never reach any of this code.

   Checks 1-4 are satisfied for real, by putting the process into the
   state the game expects. Checks 5 and 6 have no honest answer on a
   machine that is not an arcade cabinet, because the driver binaries are
   absent and the image is unsigned, so those two results are supplied
   directly. */

#include <windows.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <winternl.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "hook/pe.h"

#include "chusanhook/protect.h"
#include "util/dprintf.h"

// Check windows
#if _WIN32 || _WIN64
    #if _WIN64
        #define ENV64BIT
    #else
        #define ENV32BIT
    #endif
#endif

// Check GCC
#if __GNUC__
    #if __x86_64__ || __ppc64__
        #define ENV64BIT
    #else
        #define ENV32BIT
    #endif
#endif

#if defined(ENV32BIT)

/* Offsets into the 32-bit PEB and its process parameters. These are fixed
   by the architecture rather than by any Windows version. */

#define PEB_BEING_DEBUGGED 0x02
#define PEB_PROCESS_PARAMS 0x10
#define PP_FLAGS_OFFSET 0x08

static BYTE *chusan_peb(void)
{
    return (BYTE *) __readfsdword(0x30);
}

/* RTL_USER_PROCESS_PARAMETERS_IMAGE_KEY_MISSING */
#define PP_FLAG_IMAGE_KEY 0x4000

/* Loader handshake token. The game compares this slot against
   ror(GetCurrentProcessId(), 11) in four places and never writes it, so
   nothing maintains it on a machine that did not come up through the
   arcade loader. */
#define PEB_TOKEN_OFFSET 0xF00

/* The image carries no signature, so the game's own Authenticode check can
   only ever fail. Both halves of that check are answered for this image
   only; anything else in the process is passed through untouched. */

static wchar_t chusan_exe_path[MAX_PATH];

static bool chusan_is_self_path(const void *path)
{
    if (path == NULL || chusan_exe_path[0] == L'\0') {
        return false;
    }

    return _wcsicmp((const wchar_t *) path, chusan_exe_path) == 0;
}

/* Fake handles handed back by the CryptQueryObject answer below. Anything
   the game passes back carrying these values is its own verification
   chain; every other caller in the process has a real handle and must be
   left alone. Other components, graphics drivers in particular, run real
   Authenticode verification through these same entry points. */

#define CHUSAN_FAKE_STORE ((HCERTSTORE) (ULONG_PTR) 1)
#define CHUSAN_FAKE_MSG ((HCRYPTMSG) (ULONG_PTR) 1)

static NTSTATUS (NTAPI *next_NtQuerySystemInformation)(
        ULONG, PVOID, ULONG, PULONG);
static NTSTATUS (NTAPI *next_NtQueryInformationProcess)(
        HANDLE, ULONG, PVOID, ULONG, PULONG);
static LONG (WINAPI *next_WinVerifyTrust)(HWND, GUID *, LPVOID);
static BOOL (WINAPI *next_CryptQueryObject)(
        DWORD, const void *, DWORD, DWORD, DWORD,
        DWORD *, DWORD *, DWORD *, HCERTSTORE *, HCRYPTMSG *, const void **);
static BOOL (WINAPI *next_CryptMsgGetParam)(
        HCRYPTMSG, DWORD, DWORD, void *, DWORD *);
static PCCERT_CONTEXT (WINAPI *next_CertFindCertificateInStore)(
        HCERTSTORE, DWORD, DWORD, DWORD, const void *, PCCERT_CONTEXT);

/* Replace the address exported under name from mod. The game resolves API
   pointers by walking export tables itself, so patching the import table
   would have no effect on it. */

static void *chusan_eat_patch(
        HMODULE mod,
        const char *name,
        void *replacement)
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_DATA_DIRECTORY *dir;
    IMAGE_EXPORT_DIRECTORY *exp;
    DWORD *names;
    WORD *ords;
    DWORD *funcs;
    DWORD i;
    void *original;

    if (mod == NULL) {
        return NULL;
    }

    dos = (IMAGE_DOS_HEADER *) mod;

    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return NULL;
    }

    nt = (IMAGE_NT_HEADERS *) ((BYTE *) mod + dos->e_lfanew);

    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return NULL;
    }

    dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

    if (dir->VirtualAddress == 0 || dir->Size == 0) {
        return NULL;
    }

    exp = (IMAGE_EXPORT_DIRECTORY *) ((BYTE *) mod + dir->VirtualAddress);
    names = (DWORD *) ((BYTE *) mod + exp->AddressOfNames);
    ords = (WORD *) ((BYTE *) mod + exp->AddressOfNameOrdinals);
    funcs = (DWORD *) ((BYTE *) mod + exp->AddressOfFunctions);

    for (i = 0 ; i < exp->NumberOfNames ; i++) {
        DWORD new_rva;

        if (strcmp((char *) mod + names[i], name) != 0) {
            continue;
        }

        original = (BYTE *) mod + funcs[ords[i]];
        new_rva = (DWORD) ((BYTE *) replacement - (BYTE *) mod);

        /* Export tables live in read-only pages, so the write has to go
           through pe_patch, which handles the protection change. */

        if (FAILED(pe_patch(&funcs[ords[i]], &new_rva, sizeof(new_rva)))) {
            return NULL;
        }

        return original;
    }

    return NULL;
}

static void chusan_hook_export(
        HMODULE mod,
        const char *name,
        void *replacement,
        void **link)
{
    void *original;

    original = chusan_eat_patch(mod, name, replacement);

    if (original == NULL) {
        dprintf("Chunithm protect: Failed to hook %s\n", name);

        return;
    }

    *link = original;
}

static void chusan_fix_environment(void)
{
    /* The game walks the process environment block rather than the CRT
       copy, but SetEnvironmentVariable updates the block in place. */

    if (!SetEnvironmentVariableW(L"USERNAME", L"AppUser")) {
        dprintf("Chunithm protect: Failed to set USERNAME\n");

        return;
    }

    dprintf("Chunithm protect: USERNAME set to AppUser\n");
}

static void chusan_fix_peb_state(void)
{
    BYTE *peb;
    BYTE *pp;
    DWORD pid;
    DWORD tok;

    peb = chusan_peb();
    pp = *(BYTE **) (peb + PEB_PROCESS_PARAMS);

    if (pp == NULL) {
        return;
    }

    pid = GetCurrentProcessId();
    tok = (pid >> 11) | (pid << 21);

    *(DWORD *) (peb + PEB_TOKEN_OFFSET) = tok;

    *(DWORD *) (pp + PP_FLAGS_OFFSET) |= PP_FLAG_IMAGE_KEY;

    peb[PEB_BEING_DEBUGGED] = 0;

    dprintf("Chunithm protect: PEB token %08x\n", (unsigned) tok);
}

/* A debugger may attach after startup, which sets BeingDebugged again, so a
   single clear is not enough. */

static DWORD CALLBACK chusan_debug_scrub_thread(LPVOID param)
{
    BYTE *peb;
    DWORD sleep_ms;

    sleep_ms = (DWORD) (ULONG_PTR) param;
    peb = chusan_peb();

    for (;;) {
        peb[PEB_BEING_DEBUGGED] = 0;
        Sleep(sleep_ms);
    }

    return 0;
}

/* The module list the game scans for its driver images. The entries use the
   "\??\" form because the game copies everything after that prefix
   verbatim to build the path it compares. */

#define CHUSAN_SYSTEM_MODULE_INFORMATION 11
#define CHUSAN_SHIELD_DRIVER_COUNT 4

struct chusan_rtl_module {
    HANDLE Section;
    PVOID MappedBase;
    PVOID ImageBase;
    ULONG ImageSize;
    ULONG Flags;
    USHORT LoadOrderIndex;
    USHORT InitOrderIndex;
    USHORT LoadCount;
    USHORT OffsetToFileName;
    UCHAR FullPathName[256];
};

struct chusan_rtl_modules {
    ULONG NumberOfModules;
    struct chusan_rtl_module Modules[1];
};

static const char *const chusan_shield_drivers[CHUSAN_SHIELD_DRIVER_COUNT] = {
    "\\??\\C:\\windows\\system32\\drivers\\sgfscrypt.sys",
    "\\??\\C:\\windows\\system32\\drivers\\sghwreset.sys",
    "\\??\\C:\\windows\\system32\\drivers\\sghwmonitor.sys",
    "\\??\\C:\\windows\\system32\\drivers\\fdd.sys",
};

static NTSTATUS NTAPI chusan_hook_NtQuerySystemInformation(
        ULONG cls,
        PVOID buf,
        ULONG len,
        PULONG retlen)
{
    struct chusan_rtl_modules *mods;
    size_t need;
    ULONG i;
    NTSTATUS status;

    status = next_NtQuerySystemInformation(cls, buf, len, retlen);

    if (cls != CHUSAN_SYSTEM_MODULE_INFORMATION || status < 0 || buf == NULL) {
        return status;
    }

    mods = buf;
    need = sizeof(mods->NumberOfModules)
         + ((size_t) mods->NumberOfModules + CHUSAN_SHIELD_DRIVER_COUNT)
         * sizeof(mods->Modules[0]);

    if (len < need) {
        /* The caller's buffer cannot carry the extra entries. Leave the
           real answer untouched rather than truncating it. */

        return status;
    }

    for (i = 0 ; i < CHUSAN_SHIELD_DRIVER_COUNT ; i++) {
        struct chusan_rtl_module *mod;

        mod = &mods->Modules[mods->NumberOfModules + i];

        memset(mod, 0, sizeof(*mod));
        mod->Section = (HANDLE) 1;
        mod->MappedBase = (PVOID) 0x10000;
        mod->ImageBase = (PVOID) 0x10000;
        mod->ImageSize = 0x1000;
        mod->LoadOrderIndex = (USHORT) (mods->NumberOfModules + i);
        mod->InitOrderIndex = (USHORT) (mods->NumberOfModules + i);
        mod->LoadCount = 1;
        mod->OffsetToFileName = 4;

        strncpy((char *) mod->FullPathName,
                chusan_shield_drivers[i],
                sizeof(mod->FullPathName) - 1);
    }

    mods->NumberOfModules += CHUSAN_SHIELD_DRIVER_COUNT;

    dprintf("Chunithm protect: Added %i driver entries\n",
            CHUSAN_SHIELD_DRIVER_COUNT);

    return status;
}

static NTSTATUS NTAPI chusan_hook_NtQueryInformationProcess(
        HANDLE proc,
        ULONG cls,
        PVOID buf,
        ULONG len,
        PULONG retlen)
{
    NTSTATUS status;

    status = next_NtQueryInformationProcess(proc, cls, buf, len, retlen);

    if (cls != ProcessDebugPort || buf == NULL) {
        return status;
    }

    /* The caller passes the pseudo-handle NtCurrentProcess(), for which
       GetProcessId() is not meaningful. Treat anything that resolves to
       this process, including the pseudo-handle, as ours. */

    if (proc != (HANDLE) -1
            && proc != GetCurrentProcess()
            && GetProcessId(proc) != GetCurrentProcessId()) {
        return status;
    }

    *(DWORD *) buf = 0;

    return 0;
}

/* Pull the subject path out of whichever union member the caller used. */

static const wchar_t *chusan_wintrust_path(const WINTRUST_DATA *wtd)
{
    if (wtd == NULL) {
        return NULL;
    }

    switch (wtd->dwUnionChoice) {
    case WTD_CHOICE_FILE:
        return wtd->pFile != NULL ? wtd->pFile->pcwszFilePath : NULL;

    case WTD_CHOICE_CATALOG:
        return wtd->pCatalog != NULL ? wtd->pCatalog->pcwszMemberFilePath
                                     : NULL;

    default:
        return NULL;
    }
}

static const GUID chusan_generic_verify_v2 = {
    0x00AAC56B, 0xCD44, 0x11D0,
    { 0x8C, 0xC2, 0x00, 0xC0, 0x4F, 0xC2, 0x95, 0xEE }
};

static LONG WINAPI chusan_hook_WinVerifyTrust(
        HWND hwnd,
        GUID *action,
        LPVOID data)
{
    WINTRUST_DATA *wtd;
    const wchar_t *path;

    wtd = data;
    path = chusan_wintrust_path(wtd);

    /* Only this process's own image is answered. Other components in the
       process, graphics drivers in particular, run genuine verification
       through this same entry point and must be left alone. */

    if (action == NULL
            || memcmp(action, &chusan_generic_verify_v2, sizeof(GUID)) != 0
            || !chusan_is_self_path(path)) {
        return next_WinVerifyTrust(hwnd, action, data);
    }

    dprintf("Chunithm protect: WinVerifyTrust succeeded\n");

    return 0;
}

static BOOL WINAPI chusan_hook_CryptQueryObject(
        DWORD object_type,
        const void *object,
        DWORD expected_content,
        DWORD expected_format,
        DWORD flags,
        DWORD *msg_encoding,
        DWORD *content_type,
        DWORD *format_type,
        HCERTSTORE *store,
        HCRYPTMSG *msg,
        const void **context)
{
    if (object_type != CERT_QUERY_OBJECT_FILE
            || !chusan_is_self_path(object)) {
        return next_CryptQueryObject(object_type, object, expected_content,
                                     expected_format, flags, msg_encoding,
                                     content_type, format_type, store, msg,
                                     context);
    }

    /* Present this image as carrying an embedded PKCS#7 blob so the game
       moves on to the signer lookup, which is answered below. */

    if (msg_encoding != NULL) {
        *msg_encoding = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;
    }

    if (content_type != NULL) {
        *content_type = CERT_QUERY_CONTENT_PKCS7_SIGNED_EMBED;
    }

    if (format_type != NULL) {
        *format_type = CERT_QUERY_FORMAT_BINARY;
    }

    if (store != NULL) {
        *store = CHUSAN_FAKE_STORE;
    }

    if (msg != NULL) {
        *msg = CHUSAN_FAKE_MSG;
    }

    return TRUE;
}

static BOOL WINAPI chusan_hook_CryptMsgGetParam(
        HCRYPTMSG msg,
        DWORD param,
        DWORD index,
        void *data,
        DWORD *data_len)
{
    /* Message handles have no associated path, so the fake handle handed
       out above is what identifies the game's own chain. Real messages from
       anywhere else in the process go straight through. */

    if (msg != CHUSAN_FAKE_MSG
            || param != CMSG_SIGNER_INFO_PARAM
            || data_len == NULL) {
        return next_CryptMsgGetParam(msg, param, index, data, data_len);
    }

    /* The game asks for the size first and then allocates exactly that
       much, so any modest non-zero length is workable. */

    *data_len = 64;

    return TRUE;
}

static PCCERT_CONTEXT WINAPI chusan_hook_CertFindCertificateInStore(
        HCERTSTORE store,
        DWORD encoding,
        DWORD flags,
        DWORD find_type,
        const void *find_para,
        PCCERT_CONTEXT prev)
{
    /* Only the game's own chain uses the fake store. A real store belongs
       to some other component's verification and must be searched for
       real, so it is passed through. */

    if (store != CHUSAN_FAKE_STORE) {
        return next_CertFindCertificateInStore(store, encoding, flags,
                                               find_type, find_para, prev);
    }

    /* No issuer certificate accompanies this image, and an absent result is
       what the game accepts. */

    return NULL;
}

void chusan_protect_hook_init(void)
{
    HMODULE ntdll;
    HMODULE wintrust;
    HMODULE crypt32;
    HANDLE thread;

    GetModuleFileNameW(NULL, chusan_exe_path, _countof(chusan_exe_path));

    chusan_fix_environment();
    chusan_fix_peb_state();

    thread = CreateThread(NULL, 0, chusan_debug_scrub_thread,
                          (LPVOID) (ULONG_PTR) 200, 0, NULL);

    if (thread != NULL) {
        CloseHandle(thread);
    }

    ntdll = GetModuleHandleW(L"ntdll.dll");

    chusan_hook_export(ntdll, "NtQuerySystemInformation",
                       chusan_hook_NtQuerySystemInformation,
                       (void **) &next_NtQuerySystemInformation);

    chusan_hook_export(ntdll, "NtQueryInformationProcess",
                       chusan_hook_NtQueryInformationProcess,
                       (void **) &next_NtQueryInformationProcess);

    /* Force the crypto libraries in so the game finds them while walking
       the loader list, then redirect the entry points it drives. */

    wintrust = LoadLibraryW(L"wintrust.dll");
    crypt32 = LoadLibraryW(L"crypt32.dll");

    chusan_hook_export(wintrust, "WinVerifyTrust",
                       chusan_hook_WinVerifyTrust,
                       (void **) &next_WinVerifyTrust);

    chusan_hook_export(crypt32, "CryptQueryObject",
                       chusan_hook_CryptQueryObject,
                       (void **) &next_CryptQueryObject);

    chusan_hook_export(crypt32, "CryptMsgGetParam",
                       chusan_hook_CryptMsgGetParam,
                       (void **) &next_CryptMsgGetParam);

    chusan_hook_export(crypt32, "CertFindCertificateInStore",
                       chusan_hook_CertFindCertificateInStore,
                       (void **) &next_CertFindCertificateInStore);

    dprintf("Chunithm protect: hook enabled.\n");
}

#else

/* The protection suite only exists in the 32-bit game binary. The x64 build
   of this hook targets the daemon, which has no equivalent. */

void chusan_protect_hook_init(void)
{
}

#endif
