#include <windows.h>
#include <shlwapi.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "hooklib/reg.h"

#include "platform/registry.h"

#include "util/dprintf.h"

static HRESULT registry_store_create_parent(const wchar_t *path);

static HRESULT registry_store_create_parent_key(
        HKEY store_root,
        const wchar_t *target);

static HRESULT registry_store_ensure_directory(const wchar_t *path);

static HRESULT registry_store_open(const wchar_t *path, HKEY *root);

static const wchar_t *registry_store_root_name(HKEY root);

static HRESULT registry_store_resolve_profile(
        const struct registry_config *cfg,
        const char *default_profile,
        wchar_t *profile,
        size_t nprofile);

static bool registry_store_profile_is_valid(const wchar_t *profile);

static HRESULT registry_store_build_target(
        const wchar_t *profile,
        const wchar_t *root_name,
        const wchar_t *from_prefix,
        wchar_t **target);

static SRWLOCK registry_store_lock = SRWLOCK_INIT;
static HKEY registry_store_root;
static wchar_t *registry_store_path;

static HRESULT registry_store_open(const wchar_t *path, HKEY *root)
{
    wchar_t *path_copy;
    HKEY opened;
    LSTATUS err;
    HRESULT hr;

    assert(path != NULL);
    assert(root != NULL);

    *root = NULL;
    path_copy = NULL;

    if (path[0] == L'\0') {
        return E_INVALIDARG;
    }

    path_copy = _wcsdup(path);

    if (path_copy == NULL) {
        return E_OUTOFMEMORY;
    }

    AcquireSRWLockExclusive(&registry_store_lock);

    if (registry_store_root != NULL) {
        if (_wcsicmp(registry_store_path, path_copy) != 0) {
            dprintf(
                    "Reg store: Refusing second application hive %ls; "
                    "already using %ls\n",
                    path_copy,
                    registry_store_path);
            hr = HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);

            goto end;
        }

        *root = registry_store_root;
        hr = S_OK;

        goto end;
    }

    hr = registry_store_create_parent(path_copy);

    if (FAILED(hr)) {
        dprintf(
                "Reg store: Failed to create parent directory for %ls: "
                "%08lX\n",
                path_copy,
                (unsigned long) hr);

        goto end;
    }

    opened = NULL;
    err = RegLoadAppKeyW(
            path_copy,
            &opened,
            KEY_READ | KEY_WRITE,
            0,
            0);

    if (err != ERROR_SUCCESS) {
        dprintf(
                "Reg store: Failed to load application hive %ls: %ld\n",
                path_copy,
                (long) err);
        hr = HRESULT_FROM_WIN32(err);

        goto end;
    }

    registry_store_root = opened;
    registry_store_path = path_copy;
    path_copy = NULL;
    *root = registry_store_root;

    dprintf("Reg store: Loaded application hive %ls\n", registry_store_path);
    hr = S_OK;

end:
    ReleaseSRWLockExclusive(&registry_store_lock);
    free(path_copy);

    return hr;
}

HRESULT registry_store_push_redirect(
        const struct registry_config *cfg,
        const char *default_profile,
        HKEY from_root,
        const wchar_t *from_prefix)
{
    wchar_t profile[REGISTRY_STORE_PROFILE_MAX + 1];
    const wchar_t *root_name;
    wchar_t *target;
    HKEY store_root;
    HRESULT hr;

    assert(cfg != NULL);
    assert(default_profile != NULL);
    assert(from_root != NULL);
    assert(from_prefix != NULL);

    target = NULL;

    if (from_prefix[0] == L'\0' || from_prefix[0] == L'\\' ||
            from_prefix[wcslen(from_prefix) - 1] == L'\\') {
        return E_INVALIDARG;
    }

    switch (cfg->path_error) {
    case REGISTRY_PATH_OK:
        break;

    case REGISTRY_PATH_TOO_LONG:
        dprintf(
                "Reg store: [reg] path is too long; the resolved path must be "
                "shorter than %d characters\n",
                MAX_PATH);

        return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);

    case REGISTRY_PATH_NOT_LOCAL:
        dprintf(
                "Reg store: [reg] path must be INI-relative or drive-absolute "
                "and on a local drive (no UNC paths or network drives): %ls\n",
                cfg->path);

        return E_INVALIDARG;

    case REGISTRY_PATH_BAD_NAME:
    default:
        dprintf(
                "Reg store: [reg] path contains a device name, an invalid "
                "character or a name ending in '.' or ' ': %ls\n",
                cfg->path);

        return E_INVALIDARG;
    }

    if (cfg->profile_truncated) {
        dprintf(
                "Reg store: profile exceeds the %d-character limit\n",
                REGISTRY_STORE_PROFILE_MAX);

        return E_INVALIDARG;
    }

    root_name = registry_store_root_name(from_root);

    if (root_name == NULL) {
        dprintf("Reg store: Only predefined registry roots are supported\n");

        return E_INVALIDARG;
    }

    hr = registry_store_resolve_profile(
            cfg,
            default_profile,
            profile,
            _countof(profile));

    if (FAILED(hr)) {
        return hr;
    }

    hr = registry_store_open(cfg->path, &store_root);

    if (FAILED(hr)) {
        return hr;
    }

    hr = registry_store_build_target(
            profile,
            root_name,
            from_prefix,
            &target);

    if (FAILED(hr)) {
        return hr;
    }

    hr = registry_store_create_parent_key(store_root, target);

    if (FAILED(hr)) {
        free(target);

        return hr;
    }

    hr = reg_hook_push_redirect(
            from_root,
            from_prefix,
            store_root,
            target);

    if (SUCCEEDED(hr)) {
        dprintf(
                "Reg store: %ls\\%ls -> %ls!\\%ls\n",
                root_name,
                from_prefix,
                registry_store_path,
                target);
    }

    free(target);

    return hr;
}

static HRESULT registry_store_create_parent(const wchar_t *path)
{
    wchar_t *directory;
    HRESULT hr;

    assert(path != NULL);

    directory = _wcsdup(path);

    if (directory == NULL) {
        return E_OUTOFMEMORY;
    }

    if (!PathRemoveFileSpecW(directory) || directory[0] == L'\0') {
        hr = HRESULT_FROM_WIN32(ERROR_INVALID_NAME);

        goto end;
    }

    hr = registry_store_ensure_directory(directory);

end:
    free(directory);

    return hr;
}

static HRESULT registry_store_create_parent_key(
        HKEY store_root,
        const wchar_t *target)
{
    const wchar_t *last;
    wchar_t *parent;
    size_t parent_len;
    HKEY key;
    LSTATUS err;
    HRESULT hr;

    assert(store_root != NULL);
    assert(target != NULL);

    last = wcsrchr(target, L'\\');
    assert(last != NULL);

    parent_len = (size_t) (last - target);
    parent = malloc((parent_len + 1) * sizeof(wchar_t));

    if (parent == NULL) {
        return E_OUTOFMEMORY;
    }

    memcpy(parent, target, parent_len * sizeof(wchar_t));
    parent[parent_len] = L'\0';

    /* Keep parents non-volatile when clients create volatile keys. */
    key = NULL;
    err = RegCreateKeyExW(
            store_root,
            parent,
            0,
            NULL,
            REG_OPTION_NON_VOLATILE,
            KEY_CREATE_SUB_KEY,
            NULL,
            &key,
            NULL);

    if (err == ERROR_SUCCESS) {
        RegCloseKey(key);
        hr = S_OK;
    } else if (err == ERROR_CHILD_MUST_BE_VOLATILE) {
        dprintf(
                "Reg store: A parent of %ls is still volatile from an earlier "
                "run; it is not saved until the hive is unloaded\n",
                parent);
        hr = S_OK;
    } else {
        dprintf(
                "Reg store: Failed to create %ls: %ld\n",
                parent,
                (long) err);
        hr = HRESULT_FROM_WIN32(err);
    }

    free(parent);

    return hr;
}

static HRESULT registry_store_ensure_directory(const wchar_t *path)
{
    wchar_t *copy;
    wchar_t *cursor;
    wchar_t separator;
    DWORD attrs;
    DWORD err;
    HRESULT hr;
    size_t path_len;

    assert(path != NULL);

    if (path[0] == L'\0' || path[1] != L':' ||
            (path[2] != L'\\' && path[2] != L'/')) {
        return HRESULT_FROM_WIN32(ERROR_INVALID_NAME);
    }

    path_len = wcslen(path);
    copy = malloc((path_len + 5) * sizeof(wchar_t));

    if (copy == NULL) {
        return E_OUTOFMEMORY;
    }

    if (swprintf_s(copy, path_len + 5, L"\\\\?\\%ls", path) < 0) {
        hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);

        goto end;
    }

    for (cursor = copy + 4; *cursor != L'\0'; cursor++) {
        if (*cursor == L'/') {
            *cursor = L'\\';
        }
    }

    cursor = copy + 7;

    for (;;) {
        while (*cursor != L'\0' && *cursor != L'\\' && *cursor != L'/') {
            cursor++;
        }

        separator = *cursor;
        *cursor = L'\0';
        attrs = GetFileAttributesW(copy);

        if (attrs == INVALID_FILE_ATTRIBUTES) {
            err = GetLastError();

            if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND) {
                hr = HRESULT_FROM_WIN32(err);

                goto end;
            }

            if (!CreateDirectoryW(copy, NULL)) {
                err = GetLastError();

                if (err != ERROR_ALREADY_EXISTS && err != ERROR_FILE_EXISTS) {
                    hr = HRESULT_FROM_WIN32(err);

                    goto end;
                }
            }

            attrs = GetFileAttributesW(copy);

            if (attrs == INVALID_FILE_ATTRIBUTES) {
                err = GetLastError();
                hr = HRESULT_FROM_WIN32(
                        err != ERROR_SUCCESS ? err : ERROR_PATH_NOT_FOUND);

                goto end;
            }
        }

        if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            hr = HRESULT_FROM_WIN32(ERROR_DIRECTORY);

            goto end;
        }

        *cursor = separator;

        if (separator == L'\0') {
            break;
        }

        cursor++;
    }

    hr = S_OK;

end:
    free(copy);

    return hr;
}

static const wchar_t *registry_store_root_name(HKEY root)
{
    if (root == HKEY_LOCAL_MACHINE) {
        return L"HKLM";
    } else if (root == HKEY_CURRENT_USER) {
        return L"HKCU";
    } else if (root == HKEY_CLASSES_ROOT) {
        return L"HKCR";
    } else if (root == HKEY_USERS) {
        return L"HKU";
    } else if (root == HKEY_CURRENT_CONFIG) {
        return L"HKCC";
    } else {
        return NULL;
    }
}

static HRESULT registry_store_resolve_profile(
        const struct registry_config *cfg,
        const char *default_profile,
        wchar_t *profile,
        size_t nprofile)
{
    DWORD err;
    int nchars;

    assert(cfg != NULL);
    assert(default_profile != NULL);
    assert(profile != NULL);

    if (cfg->profile[0] != L'\0') {
        wcscpy_s(profile, nprofile, cfg->profile);
    } else {
        nchars = MultiByteToWideChar(
                CP_ACP,
                0,
                default_profile,
                -1,
                profile,
                (int) nprofile);

        if (nchars == 0) {
            err = GetLastError();

            return HRESULT_FROM_WIN32(
                    err != ERROR_SUCCESS ? err : ERROR_INVALID_NAME);
        }
    }

    if (!registry_store_profile_is_valid(profile)) {
        dprintf(
                "Reg store: Invalid profile; use 1-%d ASCII letters, "
                "digits, '.', '_' or '-'\n",
                REGISTRY_STORE_PROFILE_MAX);

        return E_INVALIDARG;
    }

    return S_OK;
}

static bool registry_store_profile_is_valid(const wchar_t *profile)
{
    wchar_t ch;
    size_t i;
    size_t len;

    assert(profile != NULL);

    len = wcslen(profile);

    if (len == 0 || len > REGISTRY_STORE_PROFILE_MAX) {
        return false;
    }

    for (i = 0; i < len; i++) {
        ch = profile[i];

        if (!((ch >= L'A' && ch <= L'Z') ||
                (ch >= L'a' && ch <= L'z') ||
                (ch >= L'0' && ch <= L'9') ||
                ch == L'.' || ch == L'_' || ch == L'-')) {
            return false;
        }
    }

    return true;
}

static HRESULT registry_store_build_target(
        const wchar_t *profile,
        const wchar_t *root_name,
        const wchar_t *from_prefix,
        wchar_t **target)
{
    static const wchar_t format[] = L"profiles\\%ls\\%ls\\%ls";
    wchar_t *result;
    size_t capacity;
    size_t from_len;
    size_t profile_len;
    size_t root_len;
    int nchars;

    assert(profile != NULL);
    assert(root_name != NULL);
    assert(from_prefix != NULL);
    assert(target != NULL);

    *target = NULL;
    profile_len = wcslen(profile);
    root_len = wcslen(root_name);
    from_len = wcslen(from_prefix);

    if (profile_len > SIZE_MAX - 12 ||
            root_len > SIZE_MAX - 12 - profile_len ||
            from_len > SIZE_MAX - 12 - profile_len - root_len) {
        return E_OUTOFMEMORY;
    }

    capacity = profile_len + root_len + from_len + 12;

    if (capacity > SIZE_MAX / sizeof(wchar_t)) {
        return E_OUTOFMEMORY;
    }

    result = malloc(capacity * sizeof(wchar_t));

    if (result == NULL) {
        return E_OUTOFMEMORY;
    }

    nchars = swprintf_s(
            result,
            capacity,
            format,
            profile,
            root_name,
            from_prefix);

    if (nchars < 0) {
        free(result);

        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }

    *target = result;

    return S_OK;
}
