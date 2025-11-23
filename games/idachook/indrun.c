#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "hook/table.h"

#include "hooklib/dll.h"

#include "util/dprintf.h"

#include "indrun.h"
#include "hooklib/spike.h"

static const wchar_t *target_modules[] = {
    L"IndRun.dll",
};

static wchar_t spike_file[MAX_PATH] = L"";

static const size_t target_modules_len = _countof(target_modules);

static void dll_hook_insert_hooks(HMODULE target);
static void app_hook_insert_hooks(HMODULE target);

static HMODULE WINAPI hook_LoadLibraryW(const wchar_t *name);
static HMODULE (WINAPI *next_LoadLibraryW)(const wchar_t *name);

static int WINAPI hook_GetSystemMetrics(int nIndex);
static int (WINAPI *next_GetSystemMetrics)(int nIndex);

static BOOL WINAPI hook_GetComputerNameW(LPWSTR  lpBuffer, LPDWORD nSize);
static DWORD WINAPI hook_GetCurrentDirectoryW( DWORD  nBufferLength, LPWSTR lpBuffer);
static BOOL WINAPI hook_GetVersionExW(LPOSVERSIONINFOW lpVersionInformation);
static int (WINAPI *next_GetVersionExW)(LPOSVERSIONINFOW lpVersionInformation);
static BOOL WINAPI hook_VerifyVersionInfoW(LPOSVERSIONINFOEXW lpVersionInformation, DWORD dwTypeMask, DWORDLONG dwlConditionMask);
static BOOL (WINAPI *next_VerifyVersionInfoW)(LPOSVERSIONINFOEXW lpVersionInformation, DWORD dwTypeMask, DWORDLONG dwlConditionMask);
static BOOL WINAPI hook_K32EnumProcesses(DWORD *lpidProcess, DWORD cb, LPDWORD lpcbNeeded);
static BOOL (WINAPI *next_K32EnumProcesses)(DWORD *lpidProcess, DWORD cb, LPDWORD lpcbNeeded);

static BOOL WINAPI hook_GetUserNameW(LPWSTR  lpBuffer, LPDWORD pcbBuffer);

static const struct hook_symbol idac_app_user32_syms[] = {
    {
        .name = "GetSystemMetrics",
        .patch = hook_GetSystemMetrics,
        .link = (void **) &next_GetSystemMetrics,
    }
};

static const struct hook_symbol idac_app_kernel32_syms[] = {
    {
        .name = "GetComputerNameW",
        .patch = hook_GetComputerNameW,
    },
    {
        .name = "GetCurrentDirectoryW",
        .patch = hook_GetCurrentDirectoryW,
    },
    {
        .name = "GetVersionExW",
        .patch = hook_GetVersionExW,
        .link = (void **) &next_GetVersionExW,
    },
    {
        .name = "VerifyVersionInfoW",
        .patch = hook_VerifyVersionInfoW,
        .link = (void **) &next_VerifyVersionInfoW,
    },
    {
        .name = "K32EnumProcesses",
        .patch = hook_K32EnumProcesses,
        .link = (void **) &next_K32EnumProcesses,
    }
};

static const struct hook_symbol idac_app_advapi32_syms[] = {
    {
        .name = "GetUserNameW",
        .patch = hook_GetUserNameW,
    }
};

static const struct hook_symbol indrun_kernel32_syms[] = {
    {
        .name = "LoadLibraryW",
        .patch = hook_LoadLibraryW,
        .link = (void **) &next_LoadLibraryW,
    }
};

void indrun_hook_init(struct indrun_config *cfg)
{
    assert(cfg != NULL);
    wchar_t full_path[MAX_PATH];

    if (!cfg->enable) {
       return;
    }

    if (wcscmp(cfg->patch_file, L"") != 0) {
        GetCurrentDirectoryW(MAX_PATH, full_path);
        swprintf_s(spike_file, MAX_PATH, L"%ls\\%ls", full_path, cfg->patch_file);
    }

    dprintf("IDAC: Hooks enabled.\n");

    // GameProject-Win64-Shipping.exe hooks
    app_hook_insert_hooks(NULL);

    // IndRun.dll hooks
    dll_hook_insert_hooks(NULL);
}

static void dll_hook_insert_hooks(HMODULE target) {
    hook_table_apply(
        target,
        "kernel32.dll",
        indrun_kernel32_syms,
        _countof(indrun_kernel32_syms));
}

void app_hook_insert_hooks(HMODULE target) {
    hook_table_apply(
        target,
        "user32.dll",
        idac_app_user32_syms,
        _countof(idac_app_user32_syms));

    hook_table_apply(
        target,
        "kernel32.dll",
        idac_app_kernel32_syms,
        _countof(idac_app_kernel32_syms));
    
    hook_table_apply(
        target,
        "advapi32.dll",
        idac_app_advapi32_syms,
        _countof(idac_app_advapi32_syms));
}

static HMODULE WINAPI hook_LoadLibraryW(const wchar_t *name)
{
    const wchar_t *name_end;
    const wchar_t *target_module;
    bool already_loaded;
    HMODULE result;
    size_t name_len;
    size_t target_module_len;

    if (name == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);

        return NULL;
    }

    // Check if the module is already loaded
    already_loaded = GetModuleHandleW(name) != NULL;

    // Must call the next handler so the DLL reference count is incremented
    result = next_LoadLibraryW(name);

    if (!already_loaded && result != NULL) {
        name_len = wcslen(name);

        for (size_t i = 0; i < target_modules_len; i++) {
            target_module = target_modules[i];
            target_module_len = wcslen(target_module);

            // Check if the newly loaded library is at least the length of
            // the name of the target module
            if (name_len < target_module_len) {
                continue;
            }

            name_end = &name[name_len - target_module_len];

            // Check if the name of the newly loaded library is one of the
            // modules the path hooks should be injected into
            if (_wcsicmp(name_end, target_module) != 0) {
                continue;
            }

            dprintf("IDAC: Hooked %S\n", target_module);

            dll_hook_insert_hooks(result);
            app_hook_insert_hooks(result);
            
            if (wcscmp(spike_file, L"") != 0) {
                spike_hook_read_config(target_module, spike_file);
            }
        }
    }

    return result;
}

static int WINAPI hook_GetSystemMetrics(int nIndex) {
    int ret = next_GetSystemMetrics(nIndex);

    // Disable mouse buttons detection
    if (nIndex == SM_CMOUSEBUTTONS) {
        dprintf("IDAC: GetSystemMetrics(%d) -> 0\n", nIndex);
        return 0;
    }

    return ret;
}

static BOOL WINAPI hook_GetComputerNameW(LPWSTR lpBuffer, LPDWORD nSize) {
    dprintf("IDAC: GetComputerNameW -> ACAE01A99999999\n");

    // Fake the computer name as ACAE01A999999999
    wcscpy(lpBuffer, L"ACAE01A999999999");
	*nSize = _countof(L"ACAE01A99999999");

    return TRUE;
}

static DWORD WINAPI hook_GetCurrentDirectoryW(DWORD nBufferLength, LPWSTR lpBuffer) {
    dprintf("IDAC: GetCurrentDirectoryW -> X:\\\n");

    // Fake the current diretory as X:
    wcscpy(lpBuffer, L"X");
    return 1;
}

static BOOL WINAPI hook_GetVersionExW(LPOSVERSIONINFOW lpVersionInformation) {
    int result = next_GetVersionExW(lpVersionInformation);

    // Fake the version as Windows 10 1809
    if (result) {
        dprintf("IDAC: GetVersionExW -> Windows 10 1809\n");
        lpVersionInformation->dwMajorVersion = 10;
        lpVersionInformation->dwMinorVersion = 0;
        lpVersionInformation->dwBuildNumber = 17763;
        return TRUE;
    }

    return result;
}

static BOOL WINAPI hook_GetUserNameW(LPWSTR lpBuffer, LPDWORD pcbBuffer) {
    dprintf("IDAC: GetUserNameW -> AppUser\n");

    // Fake the user name as AppUser
    wcscpy(lpBuffer, L"AppUser");
    *pcbBuffer = _countof(L"AppUser");

    return TRUE;
}

static BOOL WINAPI hook_VerifyVersionInfoW(LPOSVERSIONINFOEXW lpVersionInformation, DWORD dwTypeMask, DWORDLONG dwlConditionMask) {
    BOOL result = next_VerifyVersionInfoW(lpVersionInformation, dwTypeMask, dwlConditionMask);

    // Fake the version as Windows 10 1809
    if (lpVersionInformation->dwBuildNumber == 17763) {
        dprintf("IDAC: VerifyVersionInfoW -> Windows 10 1809\n");
        return TRUE;
    }

    return result;
}

static BOOL WINAPI hook_K32EnumProcesses(DWORD *lpidProcess, DWORD cb, LPDWORD lpcbNeeded) {
    BOOL result = next_K32EnumProcesses(lpidProcess, cb, lpcbNeeded);

    // Rteurn an empy process list
    dprintf("IDAC: K32EnumProcesses -> NULL\n");
    lpidProcess = NULL;
    *lpcbNeeded = 0;

    return TRUE;
}
