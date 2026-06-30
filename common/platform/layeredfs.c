#include <windows.h>

#include <stdint.h>
#include <stdbool.h>
#include <shlwapi.h>
#include <wchar.h>

#include "hook/table.h"

#include "platform/layeredfs.h"

#include "util/dprintf.h"
#include "util/lib.h"

#define layered_dprintf(l, ...) if(cfg->verbosity >= l) dprintf(__VA_ARGS__);

/* API hooks */

static HANDLE WINAPI hook_CreateFileA(
        const char *lpFileName,
        uint32_t dwDesiredAccess,
        uint32_t dwShareMode,
        SECURITY_ATTRIBUTES *lpSecurityAttributes,
        uint32_t dwCreationDisposition,
        uint32_t dwFlagsAndAttributes,
        HANDLE hTemplateFile);

static HANDLE WINAPI hook_CreateFileW(
        const wchar_t *lpFileName,
        uint32_t dwDesiredAccess,
        uint32_t dwShareMode,
        SECURITY_ATTRIBUTES *lpSecurityAttributes,
        uint32_t dwCreationDisposition,
        uint32_t dwFlagsAndAttributes,
        HANDLE hTemplateFile);

/* Link pointers */

static HANDLE (WINAPI *next_CreateFileA)(
        const char *lpFileName,
        uint32_t dwDesiredAccess,
        uint32_t dwShareMode,
        SECURITY_ATTRIBUTES *lpSecurityAttributes,
        uint32_t dwCreationDisposition,
        uint32_t dwFlagsAndAttributes,
        HANDLE hTemplateFile);

static HANDLE (WINAPI *next_CreateFileW)(
        const wchar_t *lpFileName,
        uint32_t dwDesiredAccess,
        uint32_t dwShareMode,
        SECURITY_ATTRIBUTES *lpSecurityAttributes,
        uint32_t dwCreationDisposition,
        uint32_t dwFlagsAndAttributes,
        HANDLE hTemplateFile);

/* Hook table */

static const struct hook_symbol layered_hook_syms[] = {
    {
        .name   = "CreateFileA",
        .patch  = hook_CreateFileA,
        .link   = (void **) &next_CreateFileA,
    }, {
        .name   = "CreateFileW",
        .patch  = hook_CreateFileW,
        .link   = (void **) &next_CreateFileW,
    },
};

static const struct layeredfs_config* cfg;
static wchar_t rootDirectory[MAX_PATH];
static wchar_t modDirectory[MAX_PATH];

void layeredfs_apply_hooks(HMODULE mod) {
    hook_table_apply(mod, "kernel32.dll", layered_hook_syms, _countof(layered_hook_syms));
}

bool layeredfs_current_process_is_amdaemon() {
    wchar_t* module_path = module_file_name(NULL);
    bool ret = false;

    if (module_path != NULL) {
        wchar_t* file_name = PathFindFileNameW(module_path);

        free(module_path);
        module_path = NULL;

        _wcslwr(file_name);

        if (wcsstr(file_name, L"amdaemon") != NULL) {
            ret = true;
        }
    }

    free(module_path);
    return ret;
}

HRESULT layeredfs_hook_init(const struct layeredfs_config* config) {

    cfg = config;

    if (!cfg->enable) {
        return S_FALSE;
    }

    if (!cfg->allowAmdaemon) {
        if (layeredfs_current_process_is_amdaemon()) {
            dprintf("LayeredFS: Not enabling inside amdaemon\n");
            return S_FALSE;
        }
    }

    if (!PathFileExistsW(cfg->modDirectory)) {
        dprintf("LayeredFS: Mod directory not found, disabling\n");
        return S_FALSE;
    }

    layeredfs_apply_hooks(NULL);

    if (!GetFullPathNameW(cfg->rootDirectory, MAX_PATH, rootDirectory, NULL)) {
        dprintf("LayeredFS: Failed to get root directory: %lx\n", GetLastError());
        return E_FAIL;
    }

    if (!GetFullPathNameW(cfg->modDirectory, MAX_PATH, modDirectory, NULL)) {
        dprintf("LayeredFS: Failed to get mod directory: %lx\n", GetLastError());
        return E_FAIL;
    }

    dprintf("LayeredFS: enabled\n");

    return S_OK;
}

static HANDLE WINAPI hook_CreateFileA(
        const char *lpFileName,
        uint32_t dwDesiredAccess,
        uint32_t dwShareMode,
        SECURITY_ATTRIBUTES *lpSecurityAttributes,
        uint32_t dwCreationDisposition,
        uint32_t dwFlagsAndAttributes,
        HANDLE hTemplateFile) {

    HANDLE fd = INVALID_HANDLE_VALUE;
    wchar_t* wfilename = NULL;

    if (lpFileName == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);

        goto end;
    }

    int nchars = MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, NULL, 0);
    wfilename = malloc(nchars * sizeof(wchar_t));

    if (wfilename == NULL) {
        SetLastError(ERROR_OUTOFMEMORY);

        goto end;
    }

    int result = MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, wfilename, nchars);

    if (result == 0) {
        goto end;
    }

    fd = hook_CreateFileW(
            wfilename,
            dwDesiredAccess,
            dwShareMode,
            lpSecurityAttributes,
            dwCreationDisposition, dwFlagsAndAttributes,
            hTemplateFile);

    end:
        free(wfilename);

    return fd;
}

static HANDLE WINAPI hook_CreateFileW(
        const wchar_t *lpFileName,
        uint32_t dwDesiredAccess,
        uint32_t dwShareMode,
        SECURITY_ATTRIBUTES *lpSecurityAttributes,
        uint32_t dwCreationDisposition,
        uint32_t dwFlagsAndAttributes,
        HANDLE hTemplateFile) {

    if (dwCreationDisposition != OPEN_EXISTING) {
        return next_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }

    if (lpFileName == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    // ignore DOS-style COM1-COM9
    if (wcslen(lpFileName) >= 4 && lpFileName[0] == 'C' && lpFileName[1] == 'O' && lpFileName[2] == 'M' && (lpFileName[3] >= '1' && lpFileName[3] <= '9')){
        return next_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }

    layered_dprintf(LAYEREDFS_VERBOSITY_ALL_OPENS, "LayeredFS: Open file: %ls\n", lpFileName);

    wchar_t relativePath[MAX_PATH] = {0};
    if (!PathIsRelativeW(lpFileName)) {
        // absolute paths first need to be checked if we can relativize it from where our configured mod directory is, and then if it succeeded, check if it's not pointing outside our root directory, which we simply do by checking for "..".
        if (!PathRelativePathToW(relativePath, lpFileName, 0, rootDirectory, 0) || wcsstr(relativePath, L"..") != NULL) {

            layered_dprintf(LAYEREDFS_VERBOSITY_ALL_OPENS, "LayeredFS: File is not in the configured root directory, ignoring.\n");

            return next_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
        }
    } else {
        // relative paths we can take as is, and just append to the mod directory
        wcscpy_s(relativePath, MAX_PATH, lpFileName);
    }

    WIN32_FIND_DATAW findFileData;
    wchar_t searchPath[MAX_PATH];
    swprintf_s(searchPath, MAX_PATH, L"%ls\\*", modDirectory);
    HANDLE searchHandle = FindFirstFileW(searchPath, &findFileData);

    if (searchHandle == INVALID_HANDLE_VALUE) {
        dprintf("LayeredFS: Error accessing mod directory: %lx", GetLastError());
        SetLastError(ERROR_NOT_FOUND);
        return NULL;
    }

    wchar_t selectedPath[MAX_PATH] = {0};
    do {
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (wcscmp(findFileData.cFileName, L".") != 0 && wcscmp(findFileData.cFileName, L"..") != 0) {
                wchar_t candidatePath[MAX_PATH];
                swprintf_s(candidatePath, MAX_PATH, L"%ls\\%ls\\%ls", modDirectory, findFileData.cFileName, relativePath);

                layered_dprintf(LAYEREDFS_VERBOSITY_CANDIDATES, "LayeredFS: Checking candidate: %ls\n", candidatePath);

                if (PathFileExistsW(candidatePath)) {
                    uint32_t attrs = GetFileAttributesW(candidatePath);
                    if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                        layered_dprintf(LAYEREDFS_VERBOSITY_CANDIDATES, "LayeredFS: Valid candidate: %ls\n", candidatePath);

                        wcscpy_s(selectedPath, MAX_PATH, candidatePath);
                    } else {
                        layered_dprintf(LAYEREDFS_VERBOSITY_CANDIDATES, "LayeredFS: Invalid candidate: either a directory or something else\n");
                    }
                }
            }
        }
    } while (FindNextFileW(searchHandle, &findFileData) != 0);

    if (wcslen(selectedPath) == 0) {
        layered_dprintf(LAYEREDFS_VERBOSITY_CANDIDATES, "LayeredFS: No valid candidates found to replace file.\n");

        return next_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }

    layered_dprintf(LAYEREDFS_VERBOSITY_REDIRECTS, "LayeredFS: Redirected %ls to %ls\n", lpFileName, selectedPath);

    return next_CreateFileW(selectedPath, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}