#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hook/hr.h"
#include "hook/table.h"

#include "hooklib/path.h"

#include <winioctl.h>
#include <util/dprintf.h>

/* Helpers */

static void path_hook_init(void);

/* API hooks */

static BOOL WINAPI hook_CreateDirectoryA(
        const char *lpFileName,
        SECURITY_ATTRIBUTES *lpSecurityAttributes);

static BOOL WINAPI hook_CreateDirectoryW(
        const wchar_t *lpFileName,
        SECURITY_ATTRIBUTES *lpSecurityAttributes);

static BOOL WINAPI hook_CreateDirectoryExA(
        const char *lpTemplateDirectory,
        const char *lpNewDirectory,
        SECURITY_ATTRIBUTES *lpSecurityAttributes);

static BOOL WINAPI hook_CreateDirectoryExW(
        const wchar_t *lpTemplateDirectory,
        const wchar_t *lpNewDirectory,
        SECURITY_ATTRIBUTES *lpSecurityAttributes);

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

static HANDLE WINAPI hook_FindFirstFileA(
        const char *lpFileName,
        LPWIN32_FIND_DATAA lpFindFileData);

static HANDLE WINAPI hook_FindFirstFileW(
        const wchar_t *lpFileName,
        LPWIN32_FIND_DATAW lpFindFileData);

static HANDLE WINAPI hook_FindFirstFileExA(
        const char *lpFileName,
        FINDEX_INFO_LEVELS fInfoLevelId,
        void *lpFindFileData,
        FINDEX_SEARCH_OPS fSearchOp,
        void *lpSearchFilter,
        DWORD dwAdditionalFlags);

static HANDLE WINAPI hook_FindFirstFileExW(
        const wchar_t *lpFileName,
        FINDEX_INFO_LEVELS fInfoLevelId,
        void *lpFindFileData,
        FINDEX_SEARCH_OPS fSearchOp,
        void *lpSearchFilter,
        DWORD dwAdditionalFlags);

static DWORD WINAPI hook_GetFileAttributesA(const char *lpFileName);

static DWORD WINAPI hook_GetFileAttributesW(const wchar_t *lpFileName);

static BOOL WINAPI hook_GetFileAttributesExA(
        const char *lpFileName,
        GET_FILEEX_INFO_LEVELS fInfoLevelId,
        void *lpFileInformation);

static BOOL WINAPI hook_GetFileAttributesExW(
        const wchar_t *lpFileName,
        GET_FILEEX_INFO_LEVELS fInfoLevelId,
        void *lpFileInformation);

static BOOL WINAPI hook_RemoveDirectoryA(const char *lpFileName);

static BOOL WINAPI hook_RemoveDirectoryW(const wchar_t *lpFileName);

static BOOL WINAPI hook_PathFileExistsA(LPCSTR pszPath);

static BOOL WINAPI hook_PathFileExistsW(LPCWSTR pszPath);

static BOOL WINAPI hook_MoveFileA(
        const char *lpExistingFileName,
        const char *lpNewFileName);

static BOOL WINAPI hook_MoveFileW(
        const wchar_t *lpExistingFileName,
        const wchar_t *lpNewFileName);

static BOOL WINAPI hook_CopyFileA(
        const char *lpExistingFileName,
        const char *lpNewFileName,
        BOOL bFailIfExists);

static BOOL WINAPI hook_CopyFileW(
        const wchar_t *lpExistingFileName,
        const wchar_t *lpNewFileName,
        BOOL bFailIfExists);

static BOOL WINAPI hook_MoveFileExA(
        const char *lpExistingFileName,
        const char *lpNewFileName,
        uint32_t dwFlags);

static BOOL WINAPI hook_MoveFileExW(
        const wchar_t *lpExistingFileName,
        const wchar_t *lpNewFileName,
        uint32_t dwFlags);

static BOOL WINAPI hook_CopyFileExA(
        LPCSTR             lpExistingFileName,
        LPCSTR             lpNewFileName,
        LPPROGRESS_ROUTINE lpProgressRoutine,
        LPVOID             lpData,
        LPBOOL             pbCancel,
        DWORD              dwCopyFlags
);

static BOOL WINAPI hook_CopyFileExW(
        LPCWSTR            lpExistingFileName,
        LPCWSTR            lpNewFileName,
        LPPROGRESS_ROUTINE lpProgressRoutine,
        LPVOID             lpData,
        LPBOOL             pbCancel,
        DWORD              dwCopyFlags
);

static BOOL WINAPI hook_ReplaceFileA(
        const char *lpReplacedFileName,
        const char *lpReplacementFileName,
        const char *lpBackupFileName,
        uint32_t dwReplaceFlags,
        void *lpExclude,
        void *lpReserved);

static BOOL WINAPI hook_ReplaceFileW(
        const wchar_t *lpReplacedFileName,
        const wchar_t *lpReplacementFileName,
        const wchar_t *lpBackupFileName,
        uint32_t dwReplaceFlags,
        void *lpExclude,
        void *lpReserved);

static BOOL WINAPI hook_DeleteFileA(const char *lpFileName);

static BOOL WINAPI hook_DeleteFileW(const wchar_t *lpFileName);

static DWORD WINAPI hook_GetPrivateProfileStringA(
        LPCSTR lpAppName,
        LPCSTR lpKeyName,
        LPCSTR lpDefault,
        LPSTR  lpReturnedString,
        DWORD  nSize,
        LPCSTR lpFileName
);

static DWORD WINAPI hook_GetPrivateProfileStringW(
        LPCWSTR lpAppName,
        LPCWSTR lpKeyName,
        LPCWSTR lpDefault,
        LPWSTR  lpReturnedString,
        DWORD   nSize,
        LPCWSTR lpFileName
);

static DWORD WINAPI hook_GetPrivateProfileSectionW(
        LPCWSTR lpAppName,
        LPWSTR  lpReturnedString,
        DWORD   nSize,
        LPCWSTR lpFileName
);

static UINT WINAPI hook_GetDriveTypeA(
        LPCSTR lpRootPathName
);

static UINT WINAPI hook_GetDriveTypeW(
        LPCWSTR lpRootPathName
);

static BOOL WINAPI hook_GetDiskFreeSpaceW(
        LPCWSTR lpRootPathName,
        LPDWORD lpSectorsPerCluster,
        LPDWORD lpBytesPerSector,
        LPDWORD lpNumberOfFreeClusters,
        LPDWORD lpTotalNumberOfClusters
);

static BOOL WINAPI hook_DeviceIoControl(
        HANDLE hFile,
        uint32_t dwIoControlCode,
        void *lpInBuffer,
        uint32_t nInBufferSize,
        void *lpOutBuffer,
        uint32_t nOutBufferSize,
        uint32_t *lpBytesReturned,
        OVERLAPPED *lpOverlapped);

/* Link pointers */

static BOOL (WINAPI *next_CreateDirectoryA)(
        const char *lpFileName,
        SECURITY_ATTRIBUTES *lpSecurityAttributes);

static BOOL (WINAPI *next_CreateDirectoryW)(
        const wchar_t *lpFileName,
        SECURITY_ATTRIBUTES *lpSecurityAttributes);

static BOOL (WINAPI *next_CreateDirectoryExA)(
        const char *lpTemplateDirectory,
        const char *lpNewDirectory,
        SECURITY_ATTRIBUTES *lpSecurityAttributes);

static BOOL (WINAPI *next_CreateDirectoryExW)(
        const wchar_t *lpTemplateDirectory,
        const wchar_t *lpNewDirectory,
        SECURITY_ATTRIBUTES *lpSecurityAttributes);

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

static HANDLE (WINAPI *next_FindFirstFileA)(
        const char *lpFileName,
        LPWIN32_FIND_DATAA lpFindFileData);

static HANDLE (WINAPI *next_FindFirstFileW)(
        const wchar_t *lpFileName,
        LPWIN32_FIND_DATAW lpFindFileData);

static HANDLE (WINAPI *next_FindFirstFileExA)(
        const char *lpFileName,
        FINDEX_INFO_LEVELS fInfoLevelId,
        void *lpFindFileData,
        FINDEX_SEARCH_OPS fSearchOp,
        void *lpSearchFilter,
        DWORD dwAdditionalFlags);

static HANDLE (WINAPI *next_FindFirstFileExW)(
        const wchar_t *lpFileName,
        FINDEX_INFO_LEVELS fInfoLevelId,
        void *lpFindFileData,
        FINDEX_SEARCH_OPS fSearchOp,
        void *lpSearchFilter,
        DWORD dwAdditionalFlags);

static DWORD (WINAPI *next_GetFileAttributesA)(const char *lpFileName);

static DWORD (WINAPI *next_GetFileAttributesW)(const wchar_t *lpFileName);

static BOOL (WINAPI *next_GetFileAttributesExA)(
        const char *lpFileName,
        GET_FILEEX_INFO_LEVELS fInfoLevelId,
        void *lpFileInformation);

static BOOL (WINAPI *next_GetFileAttributesExW)(
        const wchar_t *lpFileName,
        GET_FILEEX_INFO_LEVELS fInfoLevelId,
        void *lpFileInformation);

static BOOL (WINAPI *next_RemoveDirectoryA)(const char *lpFileName);

static BOOL (WINAPI *next_RemoveDirectoryW)(const wchar_t *lpFileName);

static BOOL (WINAPI *next_PathFileExistsA)(LPCSTR pszPath);

static BOOL (WINAPI *next_PathFileExistsW)(LPCWSTR pszPath);

static BOOL (WINAPI *next_MoveFileA)(
        const char *lpExistingFileName,
        const char *lpNewFileName);

static BOOL (WINAPI *next_MoveFileW)(
        const wchar_t *lpExistingFileName,
        const wchar_t *lpNewFileName);

static BOOL (WINAPI *next_CopyFileA)(
        const char *lpExistingFileName,
        const char *lpNewFileName,
        BOOL bFailIfExists);

static BOOL (WINAPI *next_CopyFileW)(
        const wchar_t *lpExistingFileName,
        const wchar_t *lpNewFileName,
        BOOL bFailIfExists);

static BOOL (WINAPI *next_MoveFileExA)(
        const char *lpExistingFileName,
        const char *lpNewFileName,
        uint32_t dwFlags);

static BOOL (WINAPI *next_MoveFileExW)(
        const wchar_t *lpExistingFileName,
        const wchar_t *lpNewFileName,
        uint32_t dwFlags);

static BOOL (WINAPI *next_CopyFileA)(
        const LPCSTR lpExistingFileName,
        const LPCSTR lpNewFileName,
        BOOL   bFailIfExists
);

static BOOL (WINAPI *next_CopyFileW)(
        const LPCWSTR  lpExistingFileName,
        const LPCWSTR  lpNewFileName,
        BOOL   bFailIfExists
);

static BOOL (WINAPI *next_CopyFileExA)(
        LPCSTR             lpExistingFileName,
        LPCSTR             lpNewFileName,
        LPPROGRESS_ROUTINE lpProgressRoutine,
        LPVOID             lpData,
        LPBOOL             pbCancel,
        DWORD              dwCopyFlags
);

static BOOL (WINAPI *next_CopyFileExW)(
        LPCWSTR            lpExistingFileName,
        LPCWSTR            lpNewFileName,
        LPPROGRESS_ROUTINE lpProgressRoutine,
        LPVOID             lpData,
        LPBOOL             pbCancel,
        DWORD              dwCopyFlags
);

static BOOL (WINAPI *next_ReplaceFileA)(
        const char *lpReplacedFileName,
        const char *lpReplacementFileName,
        const char *lpBackupFileName,
        uint32_t dwReplaceFlags,
        void *lpExclude,
        void *lpReserved);

static BOOL (WINAPI *next_ReplaceFileW)(
        const wchar_t *lpReplacedFileName,
        const wchar_t *lpReplacementFileName,
        const wchar_t *lpBackupFileName,
        uint32_t dwReplaceFlags,
        void *lpExclude,
        void *lpReserved);

static BOOL (WINAPI *next_DeleteFileA)(const char *lpFileName);

static BOOL (WINAPI *next_DeleteFileW)(const wchar_t *lpFileName);

static DWORD (WINAPI *next_GetPrivateProfileStringA)(
        LPCSTR lpAppName,
        LPCSTR lpKeyName,
        LPCSTR lpDefault,
        LPSTR  lpReturnedString,
        DWORD  nSize,
        LPCSTR lpFileName
);

static DWORD (WINAPI *next_GetPrivateProfileStringW)(
        LPCWSTR lpAppName,
        LPCWSTR lpKeyName,
        LPCWSTR lpDefault,
        LPWSTR  lpReturnedString,
        DWORD   nSize,
        LPCWSTR lpFileName
);

static DWORD (WINAPI *next_GetPrivateProfileSectionW)(
        LPCWSTR lpAppName,
        LPWSTR  lpReturnedString,
        DWORD   nSize,
        LPCWSTR lpFileName
);

static UINT (WINAPI *next_GetDriveTypeW)(
        LPCWSTR lpRootPathName
);

static UINT (WINAPI *next_GetDriveTypeA)(
        LPCSTR lpRootPathName
);

static BOOL (WINAPI *next_GetDiskFreeSpaceW)(
        LPCWSTR lpRootPathName,
        LPDWORD lpSectorsPerCluster,
        LPDWORD lpBytesPerSector,
        LPDWORD lpNumberOfFreeClusters,
        LPDWORD lpTotalNumberOfClusters
);

static BOOL (WINAPI *next_DeviceIoControl)(
        HANDLE hFile,
        uint32_t dwIoControlCode,
        void *lpInBuffer,
        uint32_t nInBufferSize,
        void *lpOutBuffer,
        uint32_t nOutBufferSize,
        uint32_t *lpBytesReturned,
        OVERLAPPED *lpOverlapped);

/* Hook table */

static const struct hook_symbol path_hook_syms[] = {
    {
        .name   = "CreateDirectoryA",
        .patch  = hook_CreateDirectoryA,
        .link   = (void **) &next_CreateDirectoryA,
    }, {
        .name   = "CreateDirectoryW",
        .patch  = hook_CreateDirectoryW,
        .link   = (void **) &next_CreateDirectoryW,
    }, {
        .name   = "CreateDirectoryExA",
        .patch  = hook_CreateDirectoryExA,
        .link   = (void **) &next_CreateDirectoryExA,
    }, {
        .name   = "CreateDirectoryExW",
        .patch  = hook_CreateDirectoryExW,
        .link   = (void **) &next_CreateDirectoryExW,
    }, {
        .name   = "CreateFileA",
        .patch  = hook_CreateFileA,
        .link   = (void **) &next_CreateFileA,
    }, {
        .name   = "CreateFileW",
        .patch  = hook_CreateFileW,
        .link   = (void **) &next_CreateFileW,
    }, {
        .name   = "FindFirstFileA",
        .patch  = hook_FindFirstFileA,
        .link   = (void **) &next_FindFirstFileA,
    }, {
        .name   = "FindFirstFileW",
        .patch  = hook_FindFirstFileW,
        .link   = (void **) &next_FindFirstFileW,
    }, {
        .name   = "FindFirstFileExA",
        .patch  = hook_FindFirstFileExA,
        .link   = (void **) &next_FindFirstFileExA,
    }, {
        .name   = "FindFirstFileExW",
        .patch  = hook_FindFirstFileExW,
        .link   = (void **) &next_FindFirstFileExW,
    }, {
        .name   = "GetFileAttributesA",
        .patch  = hook_GetFileAttributesA,
        .link   = (void **) &next_GetFileAttributesA,
    }, {
        .name   = "GetFileAttributesW",
        .patch  = hook_GetFileAttributesW,
        .link   = (void **) &next_GetFileAttributesW,
    }, {
        .name   = "GetFileAttributesExA",
        .patch  = hook_GetFileAttributesExA,
        .link   = (void **) &next_GetFileAttributesExA,
    }, {
        .name   = "GetFileAttributesExW",
        .patch  = hook_GetFileAttributesExW,
        .link   = (void **) &next_GetFileAttributesExW,
    }, {
        .name   = "RemoveDirectoryA",
        .patch  = hook_RemoveDirectoryA,
        .link   = (void **) &next_RemoveDirectoryA,
    }, {
        .name   = "RemoveDirectoryW",
        .patch  = hook_RemoveDirectoryW,
        .link   = (void **) &next_RemoveDirectoryW,
    }, {
        .name   = "PathFileExistsA",
        .patch  = hook_PathFileExistsA,
        .link   = (void **) &next_PathFileExistsA,
    }, {
        .name   = "PathFileExistsW",
        .patch  = hook_PathFileExistsW,
        .link   = (void **) &next_PathFileExistsW,
    }, {
        .name   = "MoveFileA",
        .patch  = hook_MoveFileA,
        .link   = (void **) &next_MoveFileA,
    }, {
        .name   = "MoveFileW",
        .patch  = hook_MoveFileW,
        .link   = (void **) &next_MoveFileW,
    }, {
        .name   = "MoveFileExA",
        .patch  = hook_MoveFileExA,
        .link   = (void **) &next_MoveFileExA,
    }, {
        .name   = "MoveFileExW",
        .patch  = hook_MoveFileExW,
        .link   = (void **) &next_MoveFileExW,
    }, {
        .name   = "CopyFileA",
        .patch  = hook_CopyFileA,
        .link   = (void **) &next_CopyFileA,
    }, {
        .name   = "CopyFileW",
        .patch  = hook_CopyFileW,
        .link   = (void **) &next_CopyFileW,
    }, {
        .name   = "CopyFileExA",
        .patch  = hook_CopyFileExA,
        .link   = (void **) &next_CopyFileExA,
    }, {
        .name   = "CopyFileExW",
        .patch  = hook_CopyFileW,
        .link   = (void **) &next_CopyFileExW,
    }, {
        .name   = "ReplaceFileA",
        .patch  = hook_ReplaceFileA,
        .link   = (void **) &next_ReplaceFileA,
    }, {
        .name   = "ReplaceFileW",
        .patch  = hook_ReplaceFileW,
        .link   = (void **) &next_ReplaceFileW,
    }, {
        .name   = "DeleteFileA",
        .patch  = hook_DeleteFileA,
        .link   = (void **) &next_DeleteFileA,
    }, {
        .name   = "DeleteFileW",
        .patch  = hook_DeleteFileW,
        .link   = (void **) &next_DeleteFileW,
    }, {
        .name   = "GetPrivateProfileStringA",
        .patch  = hook_GetPrivateProfileStringA,
        .link   = (void **) &next_GetPrivateProfileStringA,
    }, {
        .name   = "GetPrivateProfileStringW",
        .patch  = hook_GetPrivateProfileStringW,
        .link   = (void **) &next_GetPrivateProfileStringW,
    }, {
        .name   = "GetPrivateProfileSectionW",
        .patch  = hook_GetPrivateProfileSectionW,
        .link   = (void **) &next_GetPrivateProfileSectionW,
    }, {
        .name   = "GetDriveTypeA",
        .patch  = hook_GetDriveTypeA,
        .link = (void **) &next_GetDriveTypeA,
    }, {
        .name   = "GetDriveTypeW",
        .patch  = hook_GetDriveTypeW,
        .link = (void **) &next_GetDriveTypeW,
    }, {
        .name   = "GetDiskFreeSpaceW",
        .patch  = hook_GetDiskFreeSpaceW,
        .link = (void **) &next_GetDiskFreeSpaceW,
    }, {
        .name   = "DeviceIoControl",
        .patch  = hook_DeviceIoControl,
        .link = (void **) &next_DeviceIoControl,
    }
};

static bool path_hook_initted;
static CRITICAL_SECTION path_hook_lock;
static path_hook_t *path_hook_list;
static size_t path_hook_count;

HRESULT path_hook_push(path_hook_t hook)
{
    path_hook_t *tmp;
    HRESULT hr;

    assert(hook != NULL);

    path_hook_init();

    EnterCriticalSection(&path_hook_lock);

    tmp = realloc(
            path_hook_list,
            (path_hook_count + 1) * sizeof(path_hook_t));

    if (tmp == NULL) {
        hr = E_OUTOFMEMORY;

        goto end;
    }

    path_hook_list = tmp;
    path_hook_list[path_hook_count++] = hook;

    hr = S_OK;

end:
    LeaveCriticalSection(&path_hook_lock);

    return hr;
}

static void path_hook_init(void)
{
    /* Init is not thread safe because API hook init is not thread safe blah
       blah blah you know the drill by now. */

    if (path_hook_initted) {
        return;
    }

    path_hook_initted = true;
    InitializeCriticalSection(&path_hook_lock);

    path_hook_insert_hooks(NULL);
}

void path_hook_insert_hooks(HMODULE target)
{
    hook_table_apply(
            target,
            "kernel32.dll",
            path_hook_syms,
            _countof(path_hook_syms));
}

BOOL path_transform_a(char **out, const char *src)
{
    wchar_t *src_w;
    size_t src_c;
    wchar_t *dest_w;
    char *dest_a;
    size_t dest_s;
    BOOL ok;

    assert(out != NULL);

    src_w = NULL;
    dest_w = NULL;
    dest_a = NULL;
    *out = NULL;

    if (src == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        ok = FALSE;

        goto end;
    }

    /* Widen the path */

    mbstowcs_s(&src_c, NULL, 0, src, 0);
    src_w = malloc(src_c * sizeof(wchar_t));

    if (src_w == NULL) {
        SetLastError(ERROR_OUTOFMEMORY);
        ok = FALSE;

        goto end;
    }

    mbstowcs_s(NULL, src_w, src_c, src, src_c - 1);

    /* Try applying a path transform */

    ok = path_transform_w(&dest_w, src_w); /* Take ownership! */

    if (!ok || dest_w == NULL) {
        goto end;
    }

    /* Narrow the transformed path */

    wcstombs_s(&dest_s, NULL, 0, dest_w, 0);
    dest_a = malloc(dest_s * sizeof(char));

    if (dest_a == NULL) {
        SetLastError(ERROR_OUTOFMEMORY);
        ok = FALSE;

        goto end;
    }

    wcstombs_s(NULL, dest_a, dest_s, dest_w, dest_s - 1);

    *out = dest_a; /* Relinquish ownership to caller! */
    ok = TRUE;

end:
    free(dest_w);
    free(src_w);

    return ok;
}

BOOL path_transform_w(wchar_t **out, const wchar_t *src)
{
    BOOL ok;
    HRESULT hr;
    wchar_t *dest;
    size_t dest_c;
    size_t i;

    assert(out != NULL);

    dest = NULL;
    *out = NULL;

    EnterCriticalSection(&path_hook_lock);

    for (i = 0 ; i < path_hook_count ; i++) {
        hr = path_hook_list[i](src, NULL, &dest_c);

        if (FAILED(hr)) {
            ok = hr_propagate_win32(hr, FALSE);

            goto end;
        }

        if (hr == S_FALSE) {
            continue;
        }

        dest = malloc(dest_c * sizeof(wchar_t));

        if (dest == NULL) {
            SetLastError(ERROR_OUTOFMEMORY);
            ok = FALSE;

            goto end;
        }

        hr = path_hook_list[i](src, dest, &dest_c);

        if (FAILED(hr)) {
            ok = hr_propagate_win32(hr, FALSE);

            goto end;
        }

#if LOG_VFS
        if (!wcsstr(src, L"AppUser")) {
            dprintf("Path: %ls -> %ls\n", src, dest);
        }
#endif

        break;
    }

    *out = dest;
    dest = NULL;
    ok = TRUE;

end:
    LeaveCriticalSection(&path_hook_lock);

    free(dest);

    return ok;
}

int path_compare_w(const wchar_t *string1, const wchar_t *string2, size_t count)
{
    size_t i;
    wchar_t c1, c2;

    assert(string1 != NULL);
    assert(string2 != NULL);

    for (i = 0; i < count && string1[i] && string2[i]; i++) {
        c1 = towlower(string1[i]);

        if (c1 == '/') {
            c1 = '\\';
        }

        c2 = towlower(string2[i]);

        if (c2 == '/') {
            c2 = '\\';
        }

        if (c1 != c2) {
            break;
        }
    }

    return i == count ? 0 : string2[i] - string1[i];
}

/* Dumping ground for kernel32 file system ops whose path parameters we have to
   hook into and translate. This list will grow over time as we go back and
   fix up older games that don't pay attention to the mount point registry. */

static BOOL WINAPI hook_CreateDirectoryA(
        const char *lpFileName,
        SECURITY_ATTRIBUTES *lpSecurityAttributes)
{
    char *trans;
    BOOL ok;

    ok = path_transform_a(&trans, lpFileName);

    if (!ok) {
        return FALSE;
    }

    ok = next_CreateDirectoryA(
            trans ? trans : lpFileName,
            lpSecurityAttributes);

    free(trans);

    return ok;
}

static BOOL WINAPI hook_CreateDirectoryW(
        const wchar_t *lpFileName,
        SECURITY_ATTRIBUTES *lpSecurityAttributes)
{
    wchar_t *trans;
    BOOL ok;

    ok = path_transform_w(&trans, lpFileName);

    if (!ok) {
        return FALSE;
    }

    ok = next_CreateDirectoryW(
            trans ? trans : lpFileName,
            lpSecurityAttributes);

    free(trans);

    return ok;
}

static BOOL WINAPI hook_CreateDirectoryExA(
        const char *lpTemplateDirectory,
        const char *lpNewDirectory,
        SECURITY_ATTRIBUTES *lpSecurityAttributes)
{
    char *trans;
    BOOL ok;

    ok = path_transform_a(&trans, lpNewDirectory);

    if (!ok) {
        return FALSE;
    }

    ok = next_CreateDirectoryExA(
            lpTemplateDirectory,
            trans ? trans : lpNewDirectory,
            lpSecurityAttributes);

    free(trans);

    return ok;
}

static BOOL WINAPI hook_CreateDirectoryExW(
        const wchar_t *lpTemplateDirectory,
        const wchar_t *lpNewDirectory,
        SECURITY_ATTRIBUTES *lpSecurityAttributes)
{
    wchar_t *trans;
    BOOL ok;

    ok = path_transform_w(&trans, lpNewDirectory);

    if (!ok) {
        return FALSE;
    }

    ok = next_CreateDirectoryExW(
            lpTemplateDirectory,
            trans ? trans : lpNewDirectory,
            lpSecurityAttributes);

    free(trans);

    return ok;
}

/* Don't pull in the entire iohook framework just for CreateFileA/CreateFileW */

static HANDLE WINAPI hook_CreateFileA(
        const char *lpFileName,
        uint32_t dwDesiredAccess,
        uint32_t dwShareMode,
        SECURITY_ATTRIBUTES *lpSecurityAttributes,
        uint32_t dwCreationDisposition,
        uint32_t dwFlagsAndAttributes,
        HANDLE hTemplateFile)
{
    char *trans;
    HANDLE result;
    BOOL ok;

    ok = path_transform_a(&trans, lpFileName);

    if (!ok) {
        return INVALID_HANDLE_VALUE;
    }

    result = next_CreateFileA(
            trans ? trans : lpFileName,
            dwDesiredAccess,
            dwShareMode,
            lpSecurityAttributes,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            hTemplateFile);

    free(trans);

    return result;
}

static HANDLE WINAPI hook_CreateFileW(
        const wchar_t *lpFileName,
        uint32_t dwDesiredAccess,
        uint32_t dwShareMode,
        SECURITY_ATTRIBUTES *lpSecurityAttributes,
        uint32_t dwCreationDisposition,
        uint32_t dwFlagsAndAttributes,
        HANDLE hTemplateFile)
{
    wchar_t *trans;
    HANDLE result;
    BOOL ok;

    ok = path_transform_w(&trans, lpFileName);

    if (!ok) {
        return INVALID_HANDLE_VALUE;
    }

    result = next_CreateFileW(
            trans ? trans : lpFileName,
            dwDesiredAccess,
            dwShareMode,
            lpSecurityAttributes,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            hTemplateFile);

    free(trans);

    return result;
}

static HANDLE WINAPI hook_FindFirstFileA(
        const char *lpFileName,
        LPWIN32_FIND_DATAA lpFindFileData)
{
    char *trans;
    HANDLE result;
    BOOL ok;

    ok = path_transform_a(&trans, lpFileName);

    if (!ok) {
        return INVALID_HANDLE_VALUE;
    }

    result = next_FindFirstFileA(trans ? trans : lpFileName, lpFindFileData);

    free(trans);

    return result;
}

static HANDLE WINAPI hook_FindFirstFileW(
        const wchar_t *lpFileName,
        LPWIN32_FIND_DATAW lpFindFileData)
{
    wchar_t *trans;
    HANDLE result;
    BOOL ok;

    ok = path_transform_w(&trans, lpFileName);

    if (!ok) {
        return INVALID_HANDLE_VALUE;
    }

    result = next_FindFirstFileW(trans ? trans : lpFileName, lpFindFileData);

    free(trans);

    return result;
}

static HANDLE WINAPI hook_FindFirstFileExA(
        const char *lpFileName,
        FINDEX_INFO_LEVELS fInfoLevelId,
        void *lpFindFileData,
        FINDEX_SEARCH_OPS fSearchOp,
        void *lpSearchFilter,
        DWORD dwAdditionalFlags)
{
    char *trans;
    HANDLE result;
    BOOL ok;

    ok = path_transform_a(&trans, lpFileName);

    if (!ok) {
        return INVALID_HANDLE_VALUE;
    }

    result = next_FindFirstFileExA(
            trans ? trans : lpFileName,
            fInfoLevelId,
            lpFindFileData,
            fSearchOp,
            lpSearchFilter,
            dwAdditionalFlags);

    free(trans);

    return result;
}

static HANDLE WINAPI hook_FindFirstFileExW(
        const wchar_t *lpFileName,
        FINDEX_INFO_LEVELS fInfoLevelId,
        void *lpFindFileData,
        FINDEX_SEARCH_OPS fSearchOp,
        void *lpSearchFilter,
        DWORD dwAdditionalFlags)
{
    wchar_t *trans;
    HANDLE result;
    BOOL ok;

    ok = path_transform_w(&trans, lpFileName);

    if (!ok) {
        return INVALID_HANDLE_VALUE;
    }

    result = next_FindFirstFileExW(
            trans ? trans : lpFileName,
            fInfoLevelId,
            lpFindFileData,
            fSearchOp,
            lpSearchFilter,
            dwAdditionalFlags);

    free(trans);

    return result;
}

static DWORD WINAPI hook_GetFileAttributesA(const char *lpFileName)
{
    char *trans;
    DWORD result;
    BOOL ok;

    ok = path_transform_a(&trans, lpFileName);

    if (!ok) {
        return INVALID_FILE_ATTRIBUTES;
    }

    result = next_GetFileAttributesA(trans ? trans : lpFileName);
    free(trans);

    return result;
}

static DWORD WINAPI hook_GetFileAttributesW(const wchar_t *lpFileName)
{
    wchar_t *trans;
    DWORD result;
    BOOL ok;

    ok = path_transform_w(&trans, lpFileName);

    if (!ok) {
        return INVALID_FILE_ATTRIBUTES;
    }

    result = next_GetFileAttributesW(trans ? trans : lpFileName);

    free(trans);

    return result;
}

static BOOL WINAPI hook_GetFileAttributesExA(
        const char *lpFileName,
        GET_FILEEX_INFO_LEVELS fInfoLevelId,
        void *lpFileInformation)
{
    char *trans;
    BOOL ok;

    ok = path_transform_a(&trans, lpFileName);

    if (!ok) {
        return INVALID_FILE_ATTRIBUTES;
    }

    ok = next_GetFileAttributesExA(
            trans ? trans : lpFileName,
            fInfoLevelId,
            lpFileInformation);

    free(trans);

    return ok;
}

static BOOL WINAPI hook_GetFileAttributesExW(
        const wchar_t *lpFileName,
        GET_FILEEX_INFO_LEVELS fInfoLevelId,
        void *lpFileInformation)
{
    wchar_t *trans;
    BOOL ok;

    ok = path_transform_w(&trans, lpFileName);

    if (!ok) {
        return INVALID_FILE_ATTRIBUTES;
    }

    ok = next_GetFileAttributesExW(
            trans ? trans : lpFileName,
            fInfoLevelId,
            lpFileInformation);

    free(trans);

    return ok;
}

static BOOL WINAPI hook_RemoveDirectoryA(const char *lpFileName)
{
    char *trans;
    BOOL ok;

    ok = path_transform_a(&trans, lpFileName);

    if (!ok) {
        return FALSE;
    }

    ok = next_RemoveDirectoryA(trans ? trans : lpFileName);

    free(trans);

    return ok;
}

static BOOL WINAPI hook_RemoveDirectoryW(const wchar_t *lpFileName)
{
    wchar_t *trans;
    BOOL ok;

    ok = path_transform_w(&trans, lpFileName);

    if (!ok) {
        return FALSE;
    }

    ok = next_RemoveDirectoryW(trans ? trans : lpFileName);

    free(trans);

    return ok;
}

static BOOL WINAPI hook_PathFileExistsA(LPCSTR pszPath)
{
    char *trans;
    BOOL ok;

    ok = path_transform_a(&trans, pszPath);

    if (!ok) {
        return FALSE;
    }

    ok = next_PathFileExistsA(trans ? trans : pszPath);

    free(trans);

    return ok;
}

static BOOL WINAPI hook_PathFileExistsW(LPCWSTR pszPath)
{
    wchar_t *trans;
    BOOL ok;

    ok = path_transform_w(&trans, pszPath);

    if (!ok) {
        return FALSE;
    }

    ok = next_PathFileExistsW(trans ? trans : pszPath);

    free(trans);

    return ok;
}

static BOOL WINAPI hook_MoveFileA(
        const char *lpExistingFileName,
        const char *lpNewFileName)
{
    char *oldTrans;
    char *newTrans;
    BOOL ok;

    ok = path_transform_a(&oldTrans, lpExistingFileName);

    if (!ok) {
        return FALSE;
    }

    ok = path_transform_a(&newTrans, lpNewFileName);

    if (!ok) {
        free(oldTrans);

        return FALSE;
    }

    ok = next_MoveFileA(
        oldTrans ? oldTrans : lpExistingFileName,
        newTrans ? newTrans : lpNewFileName);

    free(oldTrans);
    free(newTrans);

    return ok;
}

static BOOL WINAPI hook_MoveFileW(
        const wchar_t *lpExistingFileName,
        const wchar_t *lpNewFileName)
{
    wchar_t *oldTrans;
    wchar_t *newTrans;
    BOOL ok;

    ok = path_transform_w(&oldTrans, lpExistingFileName);

    if (!ok) {
        return FALSE;
    }

    ok = path_transform_w(&newTrans, lpNewFileName);

    if (!ok) {
        free(oldTrans);

        return FALSE;
    }

    ok = next_MoveFileW(
        oldTrans ? oldTrans : lpExistingFileName,
        newTrans ? newTrans : lpNewFileName);

    free(oldTrans);
    free(newTrans);

    return ok;
}


static BOOL WINAPI hook_CopyFileA(
        const char *lpExistingFileName,
        const char *lpNewFileName,
        BOOL bFailIfExists)
{
    char *oldTrans;
    char *newTrans;
    BOOL ok;

    ok = path_transform_a(&oldTrans, lpExistingFileName);

    if (!ok) {
        return FALSE;
    }

    ok = path_transform_a(&newTrans, lpNewFileName);

    if (!ok) {
        free(oldTrans);

        return FALSE;
    }

    ok = next_CopyFileA(
        oldTrans ? oldTrans : lpExistingFileName,
        newTrans ? newTrans : lpNewFileName,
        bFailIfExists);

    free(oldTrans);
    free(newTrans);

    return ok;
}

static BOOL WINAPI hook_CopyFileW(
        const wchar_t *lpExistingFileName,
        const wchar_t *lpNewFileName,
        BOOL bFailIfExists)
{
    wchar_t *oldTrans;
    wchar_t *newTrans;
    BOOL ok;

    ok = path_transform_w(&oldTrans, lpExistingFileName);

    if (!ok) {
        return FALSE;
    }

    ok = path_transform_w(&newTrans, lpNewFileName);

    if (!ok) {
        free(oldTrans);

        return FALSE;
    }

    ok = next_CopyFileW(
        oldTrans ? oldTrans : lpExistingFileName,
        newTrans ? newTrans : lpNewFileName,
        bFailIfExists);

    free(oldTrans);
    free(newTrans);

    return ok;
}

static BOOL WINAPI hook_MoveFileExA(
        const char *lpExistingFileName,
        const char *lpNewFileName,
        uint32_t dwFlags)
{
    char *oldTrans;
    char *newTrans;
    BOOL ok;

    ok = path_transform_a(&oldTrans, lpExistingFileName);

    if (!ok) {
        return FALSE;
    }

    ok = path_transform_a(&newTrans, lpNewFileName);

    if (!ok) {
        free(oldTrans);

        return FALSE;
    }

    ok = next_MoveFileExA(
        oldTrans ? oldTrans : lpExistingFileName,
        newTrans ? newTrans : lpNewFileName,
        dwFlags);

    free(oldTrans);
    free(newTrans);

    return ok;
}

static BOOL WINAPI hook_MoveFileExW(
        const wchar_t *lpExistingFileName,
        const wchar_t *lpNewFileName,
        uint32_t dwFlags)
{
    wchar_t *oldTrans;
    wchar_t *newTrans;
    BOOL ok;

    ok = path_transform_w(&oldTrans, lpExistingFileName);

    if (!ok) {
        return FALSE;
    }

    ok = path_transform_w(&newTrans, lpNewFileName);

    if (!ok) {
        free(oldTrans);

        return FALSE;
    }

    ok = next_MoveFileExW(
        oldTrans ? oldTrans : lpExistingFileName,
        newTrans ? newTrans : lpNewFileName,
        dwFlags);

    free(oldTrans);
    free(newTrans);

    return ok;
}

static BOOL WINAPI hook_CopyFileExA(
        LPCSTR             lpExistingFileName,
        LPCSTR             lpNewFileName,
        LPPROGRESS_ROUTINE lpProgressRoutine,
        LPVOID             lpData,
        LPBOOL             pbCancel,
        DWORD              dwCopyFlags
)
{
    char *oldTrans;
    char *newTrans;
    BOOL ok;

    ok = path_transform_a(&oldTrans, lpExistingFileName);

    if (!ok) {
        return FALSE;
    }

    ok = path_transform_a(&newTrans, lpNewFileName);

    if (!ok) {
        free(oldTrans);

        return FALSE;
    }

    ok = next_CopyFileExA(
        oldTrans ? oldTrans : lpExistingFileName,
        newTrans ? newTrans : lpNewFileName,
        lpProgressRoutine,
        lpData,
        pbCancel,
        dwCopyFlags);

    free(oldTrans);
    free(newTrans);

    return ok;
}

static BOOL WINAPI hook_CopyFileExW(
        LPCWSTR            lpExistingFileName,
        LPCWSTR            lpNewFileName,
        LPPROGRESS_ROUTINE lpProgressRoutine,
        LPVOID             lpData,
        LPBOOL             pbCancel,
        DWORD              dwCopyFlags
)
{
    wchar_t *oldTrans;
    wchar_t *newTrans;
    BOOL ok;

    ok = path_transform_w(&oldTrans, lpExistingFileName);

    if (!ok) {
        return FALSE;
    }

    ok = path_transform_w(&newTrans, lpNewFileName);

    if (!ok) {
        free(oldTrans);

        return FALSE;
    }

    ok = next_CopyFileExW(
        oldTrans ? oldTrans : lpExistingFileName,
        newTrans ? newTrans : lpNewFileName,
        lpProgressRoutine,
        lpData,
        pbCancel,
        dwCopyFlags);

    free(oldTrans);
    free(newTrans);

    return ok;
}

static BOOL WINAPI hook_ReplaceFileA(
        const char *lpReplacedFileName,
        const char *lpReplacementFileName,
        const char *lpBackupFileName,
        uint32_t dwReplaceFlags,
        void *lpExclude,
        void *lpReserved)
{
    char *oldTrans;
    char *newTrans;
    BOOL ok;

    ok = path_transform_a(&oldTrans, lpReplacedFileName);

    if (!ok) {
        return FALSE;
    }

    ok = path_transform_a(&newTrans, lpReplacementFileName);

    if (!ok) {
        free(oldTrans);

        return FALSE;
    }

    ok = next_ReplaceFileA(
        oldTrans ? oldTrans : lpReplacedFileName,
        newTrans ? newTrans : lpReplacementFileName,
        lpBackupFileName,
        dwReplaceFlags,
        lpExclude,
        lpReserved);

    free(oldTrans);
    free(newTrans);

    return ok;
}

static BOOL WINAPI hook_ReplaceFileW(
        const wchar_t *lpReplacedFileName,
        const wchar_t *lpReplacementFileName,
        const wchar_t *lpBackupFileName,
        uint32_t dwReplaceFlags,
        void *lpExclude,
        void *lpReserved)
{
    wchar_t *oldTrans;
    wchar_t *newTrans;
    BOOL ok;

    ok = path_transform_w(&oldTrans, lpReplacedFileName);

    if (!ok) {
        return FALSE;
    }

    ok = path_transform_w(&newTrans, lpReplacementFileName);

    if (!ok) {
        free(oldTrans);

        return FALSE;
    }

    ok = next_ReplaceFileW(
        oldTrans ? oldTrans : lpReplacedFileName,
        newTrans ? newTrans : lpReplacementFileName,
        lpBackupFileName,
        dwReplaceFlags,
        lpExclude,
        lpReserved);

    free(oldTrans);
    free(newTrans);

    return ok;
}

static BOOL WINAPI hook_DeleteFileA(const char *lpFileName)
{
    char *trans;
    BOOL ok;

    ok = path_transform_a(&trans, lpFileName);

    if (!ok) {
        return FALSE;
    }

    ok = next_DeleteFileA(trans ? trans: lpFileName);

    free(trans);

    return ok;
}

static BOOL WINAPI hook_DeleteFileW(const wchar_t *lpFileName)
{
    wchar_t *trans;
    BOOL ok;

    ok = path_transform_w(&trans, lpFileName);

    if (!ok) {
        return FALSE;
    }

    ok = next_DeleteFileW(trans ? trans: lpFileName);

    free(trans);

    return ok;
}

static DWORD WINAPI hook_GetPrivateProfileStringA(
        LPCSTR lpAppName,
        LPCSTR lpKeyName,
        LPCSTR lpDefault,
        LPSTR  lpReturnedString,
        DWORD  nSize,
        LPCSTR lpFileName
) {
    char *trans;
    DWORD result;
    BOOL ok;

    ok = path_transform_a(&trans, lpFileName);

    if (!ok) {
        return FALSE;
    }

    result = next_GetPrivateProfileStringA(
        lpAppName,
        lpKeyName,
        lpDefault,
        lpReturnedString,
        nSize,
        trans ? trans: lpFileName);

    free(trans);

    return result;
}

static DWORD WINAPI hook_GetPrivateProfileStringW(
        LPCWSTR lpAppName,
        LPCWSTR lpKeyName,
        LPCWSTR lpDefault,
        LPWSTR  lpReturnedString,
        DWORD   nSize,
        LPCWSTR lpFileName
) {
    wchar_t *trans;
    DWORD result;
    BOOL ok;

    ok = path_transform_w(&trans, lpFileName);

    if (!ok) {
        return FALSE;
    }

    result = next_GetPrivateProfileStringW(
        lpAppName,
        lpKeyName,
        lpDefault,
        lpReturnedString,
        nSize, trans ? trans: lpFileName);
    
        free(trans);

    return result;
}

static DWORD WINAPI hook_GetPrivateProfileSectionW(
        LPCWSTR lpAppName,
        LPWSTR  lpReturnedString,
        DWORD   nSize,
        LPCWSTR lpFileName
) {
    wchar_t *trans;
    DWORD result;
    BOOL ok;

    ok = path_transform_w(&trans, lpFileName);

    if (!ok) {
        return FALSE;
    }

    result = next_GetPrivateProfileSectionW(
        lpAppName,
        lpReturnedString,
        nSize,
        trans ? trans: lpFileName);
    
    free(trans);
    
    return result;
}


static UINT WINAPI hook_GetDriveTypeA(
        LPCSTR lpRootPathName
) {
    char *trans;
    UINT result;
    BOOL ok;

    ok = path_transform_a(&trans, lpRootPathName);

    if (!ok) {
        return FALSE;
    }

    result = next_GetDriveTypeA(trans ? trans : lpRootPathName);

    free(trans);

    return result;
}


static UINT WINAPI hook_GetDriveTypeW(
        LPCWSTR lpRootPathName
) {
    wchar_t *trans;
    UINT result;
    BOOL ok;

    ok = path_transform_w(&trans, lpRootPathName);

    if (!ok) {
        return FALSE;
    }

    result = next_GetDriveTypeW(trans ? trans : lpRootPathName);

    free(trans);

    return result;
}

static BOOL WINAPI hook_GetDiskFreeSpaceW(
        LPCWSTR lpRootPathName,
        LPDWORD lpSectorsPerCluster,
        LPDWORD lpBytesPerSector,
        LPDWORD lpNumberOfFreeClusters,
        LPDWORD lpTotalNumberOfClusters
) {
    wchar_t *trans;
    UINT result;
    BOOL ok;

    ok = path_transform_w(&trans, lpRootPathName);

    if (!ok) {
      return FALSE;
    }

    result = next_GetDiskFreeSpaceW(trans ? trans : lpRootPathName, lpSectorsPerCluster, lpBytesPerSector, lpNumberOfFreeClusters, lpTotalNumberOfClusters);

    free(trans);

    return result;
}

static BOOL WINAPI hook_DeviceIoControl(
        HANDLE hFile,
        uint32_t dwIoControlCode,
        void *lpInBuffer,
        uint32_t nInBufferSize,
        void *lpOutBuffer,
        uint32_t nOutBufferSize,
        uint32_t *lpBytesReturned,
        OVERLAPPED *lpOverlapped) {

    if (dwIoControlCode != FSCTL_SET_REPARSE_POINT) {
        return next_DeviceIoControl(hFile, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned, lpOverlapped);
    }

    if (hFile == INVALID_HANDLE_VALUE || nInBufferSize == 0 || nInBufferSize > MAXIMUM_REPARSE_DATA_BUFFER_SIZE) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    REPARSE_DATA_BUFFER* mount = (REPARSE_DATA_BUFFER*)lpInBuffer;

    wchar_t *trans;
    wchar_t *src;

    if (wcsstr(mount->SymbolicLinkReparseBuffer.PathBuffer, L"?\\") != NULL) { // strip prefix
        src = mount->SymbolicLinkReparseBuffer.PathBuffer + 2;
    } else {
        src = mount->SymbolicLinkReparseBuffer.PathBuffer;
    }

    BOOL ok = path_transform_w(&trans, src);

    if (!ok || trans == NULL) {
        return next_DeviceIoControl(hFile, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned, lpOverlapped);
    }

    wchar_t tmp[MAX_PATH];
    swprintf_s(tmp, MAX_PATH, L"\\??\\%ls", trans);

    size_t pathLen = wcslen(tmp);
    size_t pathByteLen = pathLen * sizeof(wchar_t);

    REPARSE_DATA_BUFFER* newMount = malloc(sizeof(REPARSE_DATA_BUFFER) + pathByteLen);
    newMount->Reserved = mount->Reserved;
    newMount->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
    newMount->ReparseDataLength = 12 + pathByteLen;
    newMount->MountPointReparseBuffer.SubstituteNameOffset = 0;
    newMount->MountPointReparseBuffer.SubstituteNameLength = pathByteLen;
    newMount->MountPointReparseBuffer.PrintNameOffset = pathByteLen + 2;
    newMount->MountPointReparseBuffer.PrintNameLength = 0;
    memcpy(newMount->MountPointReparseBuffer.PathBuffer, tmp, pathByteLen);

    BOOL result = next_DeviceIoControl(hFile, dwIoControlCode, newMount, 8 + 12 + pathByteLen, lpOutBuffer, nOutBufferSize, lpBytesReturned, lpOverlapped);

    free(trans);

    return result;
}

char** str_split_a(char* a_str, const char a_delim) {
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

BOOL path_transform_args_a(const char* str, char delimiter, char* buf, size_t size) {
    assert(str != NULL);
    assert(buf != NULL);

    if (size <= 0) {
        return FALSE;
    }

    char* copy = strdup(str);
    char** tokens = str_split_a(copy, delimiter);
    char *trans;
    BOOL ok;
    BOOL failed = FALSE;

    strcpy(buf, "");

    if (tokens) {
        int j;
        for (j = 0; *(tokens + j); j++) {
            ok = path_transform_a(&trans, *(tokens + j));
            free(*(tokens + j));
            if (ok) {
                strcat_s(buf, size, trans ? trans : *(tokens+j));
                if (*(tokens + j + 1)) {
                    strcat_s(buf, size, " ");
                }
                free(trans);
            } else {
                failed = true;
            }
        }
        free(tokens);
    }

    free(copy);

    return !failed;
}

wchar_t** str_split_w(wchar_t* a_str, const wchar_t a_delim) {
    wchar_t** result    = 0;
    size_t count     = 0;
    wchar_t* tmp        = a_str;
    wchar_t* last_comma = 0;
    wchar_t delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + wcslen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(wchar_t*) * count);

    wchar_t* pt;

    if (result)
    {
        size_t idx  = 0;
        wchar_t* token = wcstok_s(a_str, delim, &pt);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = wcsdup(token);
            token = wcstok_s(0, delim, &pt);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

BOOL path_transform_args_w(const wchar_t* str, wchar_t delimiter, wchar_t* buf, size_t size) {
    assert(str != NULL);
    assert(buf != NULL);

    if (size <= 0) {
        return FALSE;
    }

    wchar_t* copy = wcsdup(str);
    wchar_t** tokens = str_split_w(copy, delimiter);
    wchar_t *trans;
    BOOL ok;
    BOOL failed = FALSE;

    wcscpy(buf, L"");

    if (tokens) {
        int j;
        for (j = 0; *(tokens + j); j++) {
            ok = path_transform_w(&trans, *(tokens + j));
            if (ok) {
                wcscat_s(buf, size, trans ? trans : *(tokens+j));
                if (*(tokens + j + 1)) {
                    wcscat_s(buf, size, L" ");
                }
                free(trans);
            } else {
                failed = true;
            }
            free(*(tokens + j));
        }
        free(tokens);
    }

    free(copy);

    return !failed;
}