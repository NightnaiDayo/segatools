#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "hook/table.h"

#include "hooklib/reg.h"
#include "hook/procaddr.h"

#include "util/dprintf.h"
#include "util/str.h"

#define MAX_CONCURRENT_REG_HANDLES 4

struct reg_hook_key {
    HKEY root;
    const wchar_t *name;
    const struct reg_hook_val *vals;
    size_t nvals;
    HKEY handles[MAX_CONCURRENT_REG_HANDLES];
    size_t nhandles;
};

struct reg_hook_redirect {
    HKEY from_root;
    wchar_t *from_prefix;
    size_t from_prefix_len;
    HKEY to_root;
    wchar_t *to_prefix;
    size_t to_prefix_len;
};

/* Helper functions */

static void reg_hook_init(void);

static BOOL CALLBACK reg_hook_init_once(
        PINIT_ONCE once,
        void *param,
        void **context);

static HRESULT reg_hook_redirect_init(void);

static BOOL CALLBACK reg_hook_redirect_init_once(
        PINIT_ONCE once,
        void *param,
        void **context);

static LRESULT reg_hook_propagate_hr(HRESULT hr);

static struct reg_hook_key *reg_hook_match_key_locked(HKEY handle);

static LSTATUS reg_hook_resolve_redirect_locked(
        HKEY parent,
        const wchar_t *name,
        HKEY *redirect_root,
        wchar_t **redirect_name);

static LSTATUS reg_hook_widen(const char *src, wchar_t **dest);

static LSTATUS reg_hook_narrow(const wchar_t *src, char **dest);

static LSTATUS reg_hook_resolve_redirect_a(
        HKEY parent,
        const char *name,
        HKEY *redirect_root,
        char **redirect_name);

static const struct reg_hook_val *reg_hook_match_val_locked(
        struct reg_hook_key *key,
        const wchar_t *name);

static LSTATUS reg_hook_open_locked(
        HKEY parent,
        const wchar_t *name,
        HKEY *out);

static LSTATUS reg_hook_query_val_locked(
        struct reg_hook_key *key,
        const wchar_t *name,
        uint32_t *type,
        void *bytes,
        uint32_t *nbytes);

/* API hooks */

static LSTATUS WINAPI hook_RegOpenKeyExW(
        HKEY parent,
        const wchar_t *name,
        uint32_t flags,
        uint32_t access,
        HKEY *out);

static LSTATUS WINAPI hook_RegOpenKeyExA(
        HKEY parent,
        const char *name,
        uint32_t flags,
        uint32_t access,
        HKEY *out);

static LSTATUS WINAPI hook_RegCreateKeyExW(
        HKEY parent,
        const wchar_t *name,
        uint32_t reserved,
        const wchar_t *class_,
        uint32_t options,
        uint32_t access,
        const SECURITY_ATTRIBUTES *sa,
        HKEY *out,
        uint32_t *disposition);

static LSTATUS WINAPI hook_RegCreateKeyExA(
        HKEY parent,
        const char *name,
        uint32_t reserved,
        char *class_,
        uint32_t options,
        uint32_t access,
        const SECURITY_ATTRIBUTES *sa,
        HKEY *out,
        uint32_t *disposition);

static LSTATUS WINAPI hook_RegCloseKey(HKEY handle);

static LSTATUS WINAPI hook_RegQueryValueExA(
        HKEY handle,
        const char *name,
        void *reserved,
        uint32_t *type,
        void *bytes,
        uint32_t *nbytes);

static LSTATUS WINAPI hook_RegQueryValueExW(
        HKEY handle,
        const wchar_t *name,
        void *reserved,
        uint32_t *type,
        void *bytes,
        uint32_t *nbytes);

static LSTATUS WINAPI hook_RegSetValueExW(
        HKEY handle,
        const wchar_t *name,
        uint32_t reserved,
        uint32_t type,
        const void *bytes,
        uint32_t nbytes);

static LSTATUS WINAPI hook_RegGetValueW(
    HKEY    hkey,
    LPCWSTR lpSubKey,
    LPCWSTR lpValue,
    uint32_t   flags,
    uint32_t *type,
    void   *pData,
    uint32_t *numData
);

static LSTATUS WINAPI hook_RegGetValueA(
    HKEY    hkey,
    LPCSTR  lpSubKey,
    LPCSTR  lpValue,
    uint32_t flags,
    uint32_t *type,
    void   *pData,
    uint32_t *numData
);

static LSTATUS WINAPI hook_RegQueryInfoKeyW(
    HKEY      hKey,
    LPWSTR    lpClass,
    LPDWORD   lpcchClass,
    LPDWORD   lpReserved,
    LPDWORD   lpcSubKeys,
    LPDWORD   lpcbMaxSubKeyLen,
    LPDWORD   lpcbMaxClassLen,
    LPDWORD   lpcValues,
    LPDWORD   lpcbMaxValueNameLen,
    LPDWORD   lpcbMaxValueLen,
    LPDWORD   lpcbSecurityDescriptor,
    PFILETIME lpftLastWriteTime);

static LSTATUS WINAPI hook_RegEnumValueW(
    HKEY    hkey,
    DWORD   dwIndex,
    LPWSTR   lpValueName,
    LPDWORD lpcchValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE  lpData,
    LPDWORD lpcbData);
/* Link pointers */

static LSTATUS (WINAPI *next_RegOpenKeyExW)(
        HKEY parent,
        const wchar_t *name,
        uint32_t flags,
        uint32_t access,
        HKEY *out);

static LSTATUS (WINAPI *next_RegOpenKeyExA)(
        HKEY parent,
        const char *name,
        uint32_t flags,
        uint32_t access,
        HKEY *out);

static LSTATUS (WINAPI *next_RegCreateKeyExW)(
        HKEY parent,
        const wchar_t *name,
        uint32_t reserved,
        const wchar_t *class_,
        uint32_t options,
        uint32_t access,
        const SECURITY_ATTRIBUTES *sa,
        HKEY *out,
        uint32_t *disposition);

static LSTATUS (WINAPI *next_RegCreateKeyExA)(
        HKEY parent,
        const char *name,
        uint32_t reserved,
        char *class_,
        uint32_t options,
        uint32_t access,
        const SECURITY_ATTRIBUTES *sa,
        HKEY *out,
        uint32_t *disposition);

static LSTATUS (WINAPI *next_RegCloseKey)(HKEY handle);

static LSTATUS (WINAPI *next_RegQueryValueExA)(
        HKEY handle,
        const char *name,
        void *reserved,
        uint32_t *type,
        void *bytes,
        uint32_t *nbytes);

static LSTATUS (WINAPI *next_RegQueryValueExW)(
        HKEY handle,
        const wchar_t *name,
        void *reserved,
        uint32_t *type,
        void *bytes,
        uint32_t *nbytes);

static LSTATUS (WINAPI *next_RegSetValueExW)(
        HKEY handle,
        const wchar_t *name,
        uint32_t reserved,
        uint32_t type,
        const void *bytes,
        uint32_t nbytes);

static LSTATUS (WINAPI *next_RegGetValueW)(
    HKEY    hkey,
    LPCWSTR lpSubKey,
    LPCWSTR lpValue,
    uint32_t   flags,
    uint32_t *type,
    void   *pData,
    uint32_t *numData
);

static LSTATUS (WINAPI *next_RegGetValueA)(
    HKEY    hkey,
    LPCSTR  lpSubKey,
    LPCSTR  lpValue,
    uint32_t flags,
    uint32_t *type,
    void   *pData,
    uint32_t *numData
);

static LSTATUS (WINAPI *next_RegQueryInfoKeyW)(
    HKEY      hKey,
    LPWSTR    lpClass,
    LPDWORD   lpcchClass,
    LPDWORD   lpReserved,
    LPDWORD   lpcSubKeys,
    LPDWORD   lpcbMaxSubKeyLen,
    LPDWORD   lpcbMaxClassLen,
    LPDWORD   lpcValues,
    LPDWORD   lpcbMaxValueNameLen,
    LPDWORD   lpcbMaxValueLen,
    LPDWORD   lpcbSecurityDescriptor,
    PFILETIME lpftLastWriteTime);

static LSTATUS (WINAPI *next_RegEnumValueW)(
    HKEY    hkey,
    DWORD   dwIndex,
    LPWSTR   lpValueName,
    LPDWORD lpcchValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE  lpData,
    LPDWORD lpcbData);

static const struct hook_symbol reg_hook_syms[] = {
    {
        .name   = "RegOpenKeyExW",
        .patch  = hook_RegOpenKeyExW,
        .link   = (void **) &next_RegOpenKeyExW,
    }, {
        .name   = "RegCreateKeyExW",
        .patch  = hook_RegCreateKeyExW,
        .link   = (void **) &next_RegCreateKeyExW,
    }, {
        .name   = "RegCloseKey",
        .patch  = hook_RegCloseKey,
        .link   = (void **) &next_RegCloseKey,
    }, {
        .name   = "RegQueryValueExA",
        .patch  = hook_RegQueryValueExA,
        .link   = (void **) &next_RegQueryValueExA,
    }, {
        .name   = "RegQueryValueExW",
        .patch  = hook_RegQueryValueExW,
        .link   = (void **) &next_RegQueryValueExW,
    }, {
        .name   = "RegSetValueExW",
        .patch  = hook_RegSetValueExW,
        .link   = (void **) &next_RegSetValueExW,
    }, {
        .name   = "RegGetValueW",
        .patch  = hook_RegGetValueW,
        .link   = (void **) &next_RegGetValueW,
    }, {
        .name   = "RegQueryInfoKeyW",
        .patch  = hook_RegQueryInfoKeyW,
        .link   = (void **) &next_RegQueryInfoKeyW,
    }, {
        .name   = "RegEnumValueW",
        .patch  = hook_RegEnumValueW,
        .link   = (void **) &next_RegEnumValueW,
    }
};

static const struct hook_symbol reg_hook_redirect_syms[] = {
    {
        .name   = "RegOpenKeyExA",
        .patch  = hook_RegOpenKeyExA,
        .link   = (void **) &next_RegOpenKeyExA,
    }, {
        .name   = "RegCreateKeyExA",
        .patch  = hook_RegCreateKeyExA,
        .link   = (void **) &next_RegCreateKeyExA,
    }, {
        .name   = "RegGetValueA",
        .patch  = hook_RegGetValueA,
        .link   = (void **) &next_RegGetValueA,
    }
};

static INIT_ONCE reg_hook_once = INIT_ONCE_STATIC_INIT;
static INIT_ONCE reg_hook_redirect_once = INIT_ONCE_STATIC_INIT;
static HRESULT reg_hook_redirect_init_hr = E_UNEXPECTED;
static CRITICAL_SECTION reg_hook_lock;
static struct reg_hook_key *reg_hook_keys;
static size_t reg_hook_nkeys;
static struct reg_hook_redirect *reg_hook_redirects;
static size_t reg_hook_nredirects;
static volatile LONG reg_hook_redirect_active;

HRESULT reg_hook_push_key(
        HKEY root,
        const wchar_t *name,
        const struct reg_hook_val *vals,
        size_t nvals)
{
    struct reg_hook_key *new_mem;
    struct reg_hook_key *new_key;
    HRESULT hr;

    assert(root != NULL);
    assert(name != NULL);
    assert(vals != NULL || nvals == 0);

    reg_hook_init();

    /*dprintf("Pushing reg key %ls:\n", name);

    for (int i = 0; i < nvals; i++) {
        dprintf("\t%ls\n", vals[i].name);
    } */
    
    EnterCriticalSection(&reg_hook_lock);

    new_mem = realloc(
            reg_hook_keys,
            (reg_hook_nkeys + 1) * sizeof(struct reg_hook_key));

    if (new_mem == NULL) {
        hr = E_OUTOFMEMORY;

        goto end;
    }

    new_key = &new_mem[reg_hook_nkeys];
    memset(new_key, 0, sizeof(*new_key));
    new_key->root = root;
    new_key->name = name; /* Expect this to be statically allocated */
    new_key->vals = vals;
    new_key->nvals = nvals;

    reg_hook_keys = new_mem;
    reg_hook_nkeys++;

    hr = S_OK;

end:
    LeaveCriticalSection(&reg_hook_lock);

    return hr;
}

HRESULT reg_hook_push_redirect(
        HKEY from_root,
        const wchar_t *from_prefix,
        HKEY to_root,
        const wchar_t *to_prefix)
{
    struct reg_hook_redirect *new_mem;
    struct reg_hook_redirect *new_redirect;
    wchar_t *from_copy;
    wchar_t *to_copy;
    size_t from_len;
    size_t to_len;
    size_t i;
    HRESULT hr;

    assert(from_root != NULL);
    assert(from_prefix != NULL);
    assert(to_root != NULL);
    assert(to_prefix != NULL);

    from_len = wcslen(from_prefix);
    to_len = wcslen(to_prefix);

    if (from_len == 0 || to_len == 0 ||
            from_prefix[0] == L'\\' || to_prefix[0] == L'\\' ||
            from_prefix[from_len - 1] == L'\\' ||
            to_prefix[to_len - 1] == L'\\') {
        return E_INVALIDARG;
    }

    reg_hook_init();
    hr = reg_hook_redirect_init();

    if (FAILED(hr)) {
        return hr;
    }

    from_copy = malloc((from_len + 1) * sizeof(wchar_t));
    to_copy = malloc((to_len + 1) * sizeof(wchar_t));

    if (from_copy == NULL || to_copy == NULL) {
        free(from_copy);
        free(to_copy);

        return E_OUTOFMEMORY;
    }

    memcpy(from_copy, from_prefix, (from_len + 1) * sizeof(wchar_t));
    memcpy(to_copy, to_prefix, (to_len + 1) * sizeof(wchar_t));

    EnterCriticalSection(&reg_hook_lock);

    for (i = 0; i < reg_hook_nredirects; i++) {
        if (reg_hook_redirects[i].from_root == from_root &&
                _wcsicmp(reg_hook_redirects[i].from_prefix, from_prefix) == 0) {
            hr = HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS);

            goto end;
        }
    }

    new_mem = realloc(
            reg_hook_redirects,
            (reg_hook_nredirects + 1) * sizeof(struct reg_hook_redirect));

    if (new_mem == NULL) {
        hr = E_OUTOFMEMORY;

        goto end;
    }

    new_redirect = &new_mem[reg_hook_nredirects];
    new_redirect->from_root = from_root;
    new_redirect->from_prefix = from_copy;
    new_redirect->from_prefix_len = from_len;
    new_redirect->to_root = to_root;
    new_redirect->to_prefix = to_copy;
    new_redirect->to_prefix_len = to_len;

    reg_hook_redirects = new_mem;
    reg_hook_nredirects++;
    InterlockedExchange(&reg_hook_redirect_active, 1);
    from_copy = NULL;
    to_copy = NULL;

    dprintf("Reg: redirect %ls\\%ls -> %ls\\%ls\n",
            from_root == HKEY_LOCAL_MACHINE ? L"HKLM" :
            from_root == HKEY_CURRENT_USER ? L"HKCU" : L"HKEY",
            from_prefix,
            to_root == HKEY_LOCAL_MACHINE ? L"HKLM" :
            to_root == HKEY_CURRENT_USER ? L"HKCU" : L"HKEY",
            to_prefix);

    hr = S_OK;

end:
    LeaveCriticalSection(&reg_hook_lock);
    free(from_copy);
    free(to_copy);

    return hr;
}

static void reg_hook_init(void)
{
    InitOnceExecuteOnce(&reg_hook_once, reg_hook_init_once, NULL, NULL);
}

static BOOL CALLBACK reg_hook_init_once(
        PINIT_ONCE once,
        void *param,
        void **context)
{
    (void) once;
    (void) param;
    (void) context;

    InitializeCriticalSection(&reg_hook_lock);
    dprintf("Reg: hook init.\n");

    reg_hook_insert_hooks(NULL);

    proc_addr_table_push(
            NULL,
            "ADVAPI32.dll",
            (struct hook_symbol *) reg_hook_syms,
            _countof(reg_hook_syms));

    return TRUE;
}

static HRESULT reg_hook_redirect_init(void)
{
    DWORD err;

    if (!InitOnceExecuteOnce(
                &reg_hook_redirect_once,
                reg_hook_redirect_init_once,
                NULL,
                NULL)) {
        err = GetLastError();

        return HRESULT_FROM_WIN32(
                err != ERROR_SUCCESS ? err : ERROR_GEN_FAILURE);
    }

    return reg_hook_redirect_init_hr;
}

static BOOL CALLBACK reg_hook_redirect_init_once(
        PINIT_ONCE once,
        void *param,
        void **context)
{
    (void) once;
    (void) param;
    (void) context;

    hook_table_apply(
            NULL,
            "advapi32.dll",
            reg_hook_redirect_syms,
            _countof(reg_hook_redirect_syms));

    reg_hook_redirect_init_hr = proc_addr_table_push(
            NULL,
            "ADVAPI32.dll",
            (struct hook_symbol *) reg_hook_redirect_syms,
            _countof(reg_hook_redirect_syms));

    return TRUE;
}

void reg_hook_insert_hooks(HMODULE target)
{
    hook_table_apply(
            target,
            "advapi32.dll",
            reg_hook_syms,
            _countof(reg_hook_syms));

    if (InterlockedCompareExchange(&reg_hook_redirect_active, 0, 0) != 0) {
        hook_table_apply(
                target,
                "advapi32.dll",
                reg_hook_redirect_syms,
                _countof(reg_hook_redirect_syms));
    }
}

static LRESULT reg_hook_propagate_hr(HRESULT hr)
{
    if (SUCCEEDED(hr)) {
        return ERROR_SUCCESS;
    } else if (HRESULT_FACILITY(hr) == FACILITY_WIN32) {
        return HRESULT_CODE(hr);
    } else {
        return ERROR_GEN_FAILURE;
    }
}

static struct reg_hook_key *reg_hook_match_key_locked(HKEY handle)
{
    struct reg_hook_key *key;
    size_t i, j;

    if (handle == NULL || handle == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    for (i = 0 ; i < reg_hook_nkeys ; i++) {
        key = &reg_hook_keys[i];

        for (j = 0; j < key->nhandles; j++) {
            if (key->handles[j] == handle) {
                return key;
            }
        }
    }

    return NULL;
}

static LSTATUS reg_hook_resolve_redirect_locked(
        HKEY parent,
        const wchar_t *name,
        HKEY *redirect_root,
        wchar_t **redirect_name)
{
    const struct reg_hook_redirect *best;
    const struct reg_hook_redirect *redirect;
    const wchar_t *suffix;
    wchar_t *mapped;
    size_t mapped_len;
    size_t name_len;
    size_t suffix_len;
    size_t i;

    assert(redirect_root != NULL);
    assert(redirect_name != NULL);

    *redirect_root = NULL;
    *redirect_name = NULL;

    if (name == NULL) {
        return ERROR_SUCCESS;
    }

    name_len = wcslen(name);
    best = NULL;

    for (i = 0; i < reg_hook_nredirects; i++) {
        redirect = &reg_hook_redirects[i];

        if (redirect->from_root != parent ||
                name_len < redirect->from_prefix_len ||
                _wcsnicmp(
                        name,
                        redirect->from_prefix,
                        redirect->from_prefix_len) != 0) {
            continue;
        }

        if (name[redirect->from_prefix_len] != L'\0' &&
                name[redirect->from_prefix_len] != L'\\') {
            continue;
        }

        if (best == NULL ||
                redirect->from_prefix_len > best->from_prefix_len) {
            best = redirect;
        }
    }

    if (best == NULL) {
        return ERROR_SUCCESS;
    }

    suffix = name + best->from_prefix_len;
    suffix_len = name_len - best->from_prefix_len;

    if (best->to_prefix_len > SIZE_MAX - suffix_len - 1) {
        return ERROR_OUTOFMEMORY;
    }

    mapped_len = best->to_prefix_len + suffix_len;
    mapped = malloc((mapped_len + 1) * sizeof(wchar_t));

    if (mapped == NULL) {
        return ERROR_OUTOFMEMORY;
    }

    memcpy(
            mapped,
            best->to_prefix,
            best->to_prefix_len * sizeof(wchar_t));
    memcpy(
            mapped + best->to_prefix_len,
            suffix,
            (suffix_len + 1) * sizeof(wchar_t));

    *redirect_root = best->to_root;
    *redirect_name = mapped;

    return ERROR_SUCCESS;
}

static LSTATUS reg_hook_widen(const char *src, wchar_t **dest)
{
    wchar_t *result;
    DWORD err;
    int nchars;

    assert(dest != NULL);

    *dest = NULL;

    if (src == NULL) {
        return ERROR_SUCCESS;
    }

    nchars = MultiByteToWideChar(CP_ACP, 0, src, -1, NULL, 0);

    if (nchars == 0) {
        err = GetLastError();

        return err != ERROR_SUCCESS ? err : ERROR_NO_UNICODE_TRANSLATION;
    }

    result = malloc(nchars * sizeof(wchar_t));

    if (result == NULL) {
        return ERROR_OUTOFMEMORY;
    }

    if (MultiByteToWideChar(CP_ACP, 0, src, -1, result, nchars) == 0) {
        err = GetLastError();
        free(result);

        return err != ERROR_SUCCESS ? err : ERROR_NO_UNICODE_TRANSLATION;
    }

    *dest = result;

    return ERROR_SUCCESS;
}

static LSTATUS reg_hook_narrow(const wchar_t *src, char **dest)
{
    BOOL used_default;
    BOOL *used_default_ptr;
    char *result;
    DWORD err;
    DWORD flags;
    int nchars;

    assert(dest != NULL);

    *dest = NULL;

    if (src == NULL) {
        return ERROR_SUCCESS;
    }

    if (GetACP() == CP_UTF8) {
        flags = 0;
        used_default_ptr = NULL;
    } else {
        flags = WC_NO_BEST_FIT_CHARS;
        used_default_ptr = &used_default;
    }

    used_default = FALSE;
    nchars = WideCharToMultiByte(
            CP_ACP,
            flags,
            src,
            -1,
            NULL,
            0,
            NULL,
            used_default_ptr);

    if (nchars == 0) {
        err = GetLastError();

        return err != ERROR_SUCCESS ? err : ERROR_NO_UNICODE_TRANSLATION;
    }

    if (used_default_ptr != NULL && used_default) {
        return ERROR_NO_UNICODE_TRANSLATION;
    }

    result = malloc(nchars);

    if (result == NULL) {
        return ERROR_OUTOFMEMORY;
    }

    used_default = FALSE;

    if (WideCharToMultiByte(
                CP_ACP,
                flags,
                src,
                -1,
                result,
                nchars,
                NULL,
                used_default_ptr) == 0 ||
            (used_default_ptr != NULL && used_default)) {
        err = GetLastError();
        free(result);

        return err != ERROR_SUCCESS ? err : ERROR_NO_UNICODE_TRANSLATION;
    }

    *dest = result;

    return ERROR_SUCCESS;
}

static LSTATUS reg_hook_resolve_redirect_a(
        HKEY parent,
        const char *name,
        HKEY *redirect_root,
        char **redirect_name)
{
    wchar_t *name_w;
    wchar_t *mapped;
    LSTATUS err;

    *redirect_root = NULL;
    *redirect_name = NULL;
    mapped = NULL;

    err = reg_hook_widen(name, &name_w);

    if (err != ERROR_SUCCESS) {
        return err;
    }

    EnterCriticalSection(&reg_hook_lock);
    err = reg_hook_resolve_redirect_locked(
            parent, name_w, redirect_root, &mapped);
    LeaveCriticalSection(&reg_hook_lock);
    free(name_w);

    if (err == ERROR_SUCCESS && mapped != NULL) {
        err = reg_hook_narrow(mapped, redirect_name);
    }

    free(mapped);

    return err;
}

static const struct reg_hook_val *reg_hook_match_val_locked(
        struct reg_hook_key *key,
        const wchar_t *name)
{
    const struct reg_hook_val *val;
    size_t i;

    /* Watch out for accesses to the key's default value */

    if (name == NULL) {
        name = L"";
    }

    for (i = 0 ; i < key->nvals ; i++) {
        val = &key->vals[i];

        if (wstr_ieq(val->name, name)) {
            return val;
        }
    }

    return NULL;
}

static LSTATUS reg_hook_open_locked(
        HKEY parent,
        const wchar_t *name,
        HKEY *out)
{
    struct reg_hook_key *key;
    LSTATUS err;
    size_t i;

    *out = NULL;

    if (name == NULL) {
        return ERROR_SUCCESS;
    }

    for (i = 0 ; i < reg_hook_nkeys ; i++) {
        /* Assume reg keys are referenced from a root key and not from some
           intermediary key */
        key = &reg_hook_keys[i];
        //dprintf("Reg: %ls vs %ls\n", name, key->name);

        if (key->root == parent && wstr_ieq(key->name, name)) {
            break;
        }
    }

    /* (Bail out if we didn't find anything; this causes the open/create call
       to be passed onward down the hook chain) */

    if (i >= reg_hook_nkeys) {
        return ERROR_SUCCESS;
    }

    /* Assume only MAX_CONCURRENT_REG_HANDLES handles will be open at a time */

    if (key->nhandles >= MAX_CONCURRENT_REG_HANDLES) {
        return ERROR_SHARING_VIOLATION;
    }

    /* Open a unique HKEY handle that we can use to identify accesses to
       this virtual registry key. We open a read-only handle to an arbitrary
       registry key that we can reliably assume exists and isn't one of the
       hardcoded root handles. HKLM\SOFTWARE will suffice for this purpose. */

    err = next_RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE",
            0,
            KEY_READ,
            out);

    if (err == ERROR_SUCCESS) {
        key->handles[key->nhandles++] = *out;
    }

    return err;
}

static LSTATUS WINAPI hook_RegOpenKeyExW(
        HKEY parent,
        const wchar_t *name,
        uint32_t flags,
        uint32_t access,
        HKEY *out)
{
    HKEY redirect_root;
    wchar_t *redirect_name;
    LSTATUS err;

    if (out == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    *out = NULL;
    redirect_name = NULL;

    EnterCriticalSection(&reg_hook_lock);
    err = reg_hook_resolve_redirect_locked(
            parent,
            name,
            &redirect_root,
            &redirect_name);

    if (err == ERROR_SUCCESS && redirect_name == NULL) {
        err = reg_hook_open_locked(parent, name, out);
    }

    LeaveCriticalSection(&reg_hook_lock);

    if (err != ERROR_SUCCESS) {
        free(redirect_name);

        return err;
    }

    if (redirect_name != NULL) {
        err = next_RegOpenKeyExW(
                redirect_root,
                redirect_name,
                flags,
                access,
                out);
        free(redirect_name);

        return err;
    }

    if (err == ERROR_SUCCESS) {
        if (*out != NULL) {
            //dprintf("Registry: Opened virtual key %S\n", name);
        } else {
            err = next_RegOpenKeyExW(parent, name, flags, access, out);
        }
    }

    return err;
}

static LSTATUS WINAPI hook_RegOpenKeyExA(
        HKEY parent,
        const char *name,
        uint32_t flags,
        uint32_t access,
        HKEY *out)
{
    char *redirect_name;
    HKEY redirect_root;
    LSTATUS err;

    if (InterlockedCompareExchange(&reg_hook_redirect_active, 0, 0) == 0) {
        return next_RegOpenKeyExA(parent, name, flags, access, out);
    }

    if (out == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    *out = NULL;
    err = reg_hook_resolve_redirect_a(
            parent, name, &redirect_root, &redirect_name);

    if (err != ERROR_SUCCESS) {
        return err;
    }

    if (redirect_name != NULL) {
        parent = redirect_root;
        name = redirect_name;
    }

    err = next_RegOpenKeyExA(parent, name, flags, access, out);
    free(redirect_name);

    return err;
}

static LSTATUS WINAPI hook_RegCreateKeyExW(
        HKEY parent,
        const wchar_t *name,
        uint32_t reserved,
        const wchar_t *class_,
        uint32_t options,
        uint32_t access,
        const SECURITY_ATTRIBUTES *sa,
        HKEY *out,
        uint32_t *disposition)
{
    HKEY redirect_root;
    wchar_t *redirect_name;
    LSTATUS err;

    if (out == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    *out = NULL;
    redirect_name = NULL;

    EnterCriticalSection(&reg_hook_lock);
    err = reg_hook_resolve_redirect_locked(
            parent,
            name,
            &redirect_root,
            &redirect_name);

    if (err == ERROR_SUCCESS && redirect_name == NULL) {
        err = reg_hook_open_locked(parent, name, out);
    }

    LeaveCriticalSection(&reg_hook_lock);

    if (err != ERROR_SUCCESS) {
        free(redirect_name);

        return err;
    }

    if (redirect_name != NULL) {
        err = next_RegCreateKeyExW(
                redirect_root,
                redirect_name,
                reserved,
                class_,
                options,
                access,
                sa,
                out,
                disposition);
        free(redirect_name);

        return err;
    }

    if (err == ERROR_SUCCESS) {
        if (*out != NULL) {
            //dprintf("Registry: Created virtual key %S\n", name);
        } else {
            err = next_RegCreateKeyExW(
                    parent,
                    name,
                    reserved,
                    class_,
                    options,
                    access,
                    sa,
                    out,
                    disposition);
        }
    }

    return err;
}

static LSTATUS WINAPI hook_RegCreateKeyExA(
        HKEY parent,
        const char *name,
        uint32_t reserved,
        char *class_,
        uint32_t options,
        uint32_t access,
        const SECURITY_ATTRIBUTES *sa,
        HKEY *out,
        uint32_t *disposition)
{
    char *redirect_name;
    HKEY redirect_root;
    LSTATUS err;

    if (InterlockedCompareExchange(&reg_hook_redirect_active, 0, 0) == 0) {
        return next_RegCreateKeyExA(
                parent,
                name,
                reserved,
                class_,
                options,
                access,
                sa,
                out,
                disposition);
    }

    if (out == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    *out = NULL;
    err = reg_hook_resolve_redirect_a(
            parent, name, &redirect_root, &redirect_name);

    if (err != ERROR_SUCCESS) {
        return err;
    }

    if (redirect_name != NULL) {
        parent = redirect_root;
        name = redirect_name;
    }

    err = next_RegCreateKeyExA(
            parent,
            name,
            reserved,
            class_,
            options,
            access,
            sa,
            out,
            disposition);
    free(redirect_name);

    return err;
}

static LSTATUS WINAPI hook_RegCloseKey(HKEY handle)
{
    struct reg_hook_key *key;
    size_t i, j, k;

    EnterCriticalSection(&reg_hook_lock);

    for (i = 0 ; i < reg_hook_nkeys ; i++) {
        key = &reg_hook_keys[i];

        for (j = 0; j < key->nhandles; j++) {
            if (key->handles[j] == handle) {
                for (k = j; k < MAX_CONCURRENT_REG_HANDLES - 1; k++) {
                    key->handles[k] = key->handles[k + 1];
                }
                key->handles[MAX_CONCURRENT_REG_HANDLES - 1] = NULL;
                key->nhandles--;
                //dprintf("Registry: Closed virtual key %S\n", key->name);
            }
        }
    }

    LeaveCriticalSection(&reg_hook_lock);

    return next_RegCloseKey(handle);
}

static LSTATUS WINAPI hook_RegQueryValueExW(
        HKEY handle,
        const wchar_t *name,
        void *reserved,
        uint32_t *type,
        void *bytes,
        uint32_t *nbytes)
{
    struct reg_hook_key *key;
    LSTATUS err;

    EnterCriticalSection(&reg_hook_lock);

    key = reg_hook_match_key_locked(handle);

    /* Check if this is a virtualized registry key */

    if (key == NULL) {
        LeaveCriticalSection(&reg_hook_lock);

        return next_RegQueryValueExW(
                handle,
                name,
                reserved,
                type,
                bytes,
                nbytes);
    }

    /* Call the factored out core of this function because RegQueryValueExA
       has to be a blight upon my existence */

    err = reg_hook_query_val_locked(key, name, type, bytes, nbytes);

    LeaveCriticalSection(&reg_hook_lock);

    return err;
}

/* now this right here is a pain in my ass */

static LSTATUS WINAPI hook_RegQueryValueExA(
        HKEY handle,
        const char *name,
        void *reserved,
        uint32_t *type,
        void *bytes,
        uint32_t *nbytes)
{
    /* _s: sizeof, _c: _countof(), _w: widened */

    struct reg_hook_key *key;
    wchar_t *name_w;
    size_t name_c;
    wchar_t *content;
    uint32_t content_s;
    size_t content_c;
    uint32_t type_site;
    LSTATUS err;

    name_w = NULL;
    content = NULL;

    /* Normalize inconvenient inputs */

    if (name == NULL) {
        name = "";
    }

    if (type == NULL) {
        type = &type_site;
    }

    /* Look up key handle, early exit if no match */

    EnterCriticalSection(&reg_hook_lock);
    key = reg_hook_match_key_locked(handle);

    if (key == NULL) {
        LeaveCriticalSection(&reg_hook_lock);

        return next_RegQueryValueExA(
                handle,
                name,
                reserved,
                type,
                bytes,
                nbytes);
    }

    /* OK, first off we need to widen the name. This requires a temporary
       buffer allocation. */

    mbstowcs_s(&name_c, NULL, 0, name, 0);
    name_w = malloc(name_c * sizeof(wchar_t));

    if (name_w == NULL) {
        err = ERROR_OUTOFMEMORY;

        goto end;
    }

    mbstowcs_s(NULL, name_w, name_c, name, name_c - 1);

    /* Next, check to see if the caller even cares about the content. We can
       pass through if they don't. */

    if (bytes == NULL && nbytes == NULL) {
        err = reg_hook_query_val_locked(key, name_w, type, NULL, NULL);

        goto end;
    }

    /* Next, we need to check the key type to see if it's REG_SZ. */

    err = reg_hook_query_val_locked(key, name_w, type, NULL, NULL);

    if (err != ERROR_SUCCESS) {
        goto end;
    }

    /* If it is not REG_SZ then pass the content directly.
       (We ignore the REG_MULTI_SZ case here). */

    assert(*type != REG_MULTI_SZ);

    if (*type != REG_SZ) {
        err = reg_hook_query_val_locked(key, name_w, type, bytes, nbytes);

        goto end;
    }

    /* Otherwise things get more complicated. First we must measure the wide-
       character length of the value (hopefully said value does not change
       under our feet, of course). */

    err = reg_hook_query_val_locked(key, name_w, type, NULL, &content_s);

    if (err != ERROR_SUCCESS) {
        goto end;
    }

    /* Next, allocate a scratch buffer. Even if the caller doesn't supply an
       output buffer we need to know the actual content to be able to size the
       narrow version. */

    content = malloc(content_s);

    if (content == NULL) {
        err = ERROR_OUTOFMEMORY;

        goto end;
    }

    /* Get the data... */

    err = reg_hook_query_val_locked(key, name_w, type, content, &content_s);

    if (err != ERROR_SUCCESS) {
        goto end;
    }

    /* Now size the corresponding narrow form and return it to the caller */

    wcstombs_s(&content_c, NULL, 0, content, 0);

    if (bytes != NULL) {
        if (nbytes == NULL) {
            err = ERROR_INVALID_PARAMETER;

            goto end;
        }

        if (*nbytes < content_c) {
            err = ERROR_MORE_DATA;

            goto end;
        }

        wcstombs_s(NULL, bytes, *nbytes, content, content_c - 1);
    }

    if (nbytes != NULL) { /* It really should be, based on earlier checks ... */
        *nbytes = content_c;
    }

    err = ERROR_SUCCESS;

end:
    LeaveCriticalSection(&reg_hook_lock);

    free(content);
    free(name_w);

    return err;
}

static LSTATUS reg_hook_query_val_locked(
        struct reg_hook_key *key,
        const wchar_t *name,
        uint32_t *type,
        void *bytes,
        uint32_t *nbytes)
{
    const struct reg_hook_val *val;
    LSTATUS err;
    HRESULT hr;

    val = reg_hook_match_val_locked(key, name);

    if (val != NULL) {
        if (type != NULL) {
            *type = val->type;
        }

        if (val->read != NULL) {
            hr = val->read(bytes, nbytes);
            err = reg_hook_propagate_hr(hr);
        } else {
            dprintf("Registry: %S: Val %S has no read handler\n",
                    key->name,
                    name);

            err = ERROR_ACCESS_DENIED;
        }
    } else {
        dprintf("Registry: Key %S: Val %S not found\n", key->name, name);
        err = ERROR_FILE_NOT_FOUND;
    }

    return err;
}

static LSTATUS WINAPI hook_RegSetValueExW(
        HKEY handle,
        const wchar_t *name,
        uint32_t reserved,
        uint32_t type,
        const void *bytes,
        uint32_t nbytes)
{
    struct reg_hook_key *key;
    const struct reg_hook_val *val;
    LSTATUS err;
    HRESULT hr;

    EnterCriticalSection(&reg_hook_lock);

    key = reg_hook_match_key_locked(handle);

    if (key == NULL) {
        LeaveCriticalSection(&reg_hook_lock);

        return next_RegSetValueExW(
                handle,
                name,
                reserved,
                type,
                bytes,
                nbytes);
    }

    val = reg_hook_match_val_locked(key, name);

    if (val != NULL) {
        if (val->write != NULL) {
            if (type != val->type) {
                dprintf(        "Registry: Key %S: Val %S: Type mismatch "
                                "(expected %i got %i)\n",
                        key->name,
                        name,
                        val->type,
                        type);

                err = ERROR_ACCESS_DENIED;
            } else {
                dprintf("Registry: Write virtual key %S value %S\n",
                        key->name,
                        val->name);

                hr = val->write(bytes, nbytes);
                err = reg_hook_propagate_hr(hr);
            }
        } else {
            /* No write handler (the common case), black-hole whatever gets
               written. */

            err = ERROR_SUCCESS;
        }
    } else {
        dprintf("Registry: Key %S: Val %S not found\n", key->name, name);
        err = ERROR_FILE_NOT_FOUND;
    }

    LeaveCriticalSection(&reg_hook_lock);

    return err;
}

static LSTATUS WINAPI hook_RegGetValueW(
    HKEY    handle,
    LPCWSTR subkey,
    LPCWSTR name,
    uint32_t   flags,
    uint32_t *type,
    void   *pData,
    uint32_t *numData)
{
    struct reg_hook_key *key;
    HKEY tmp = NULL;
    const struct reg_hook_val *val;
    HKEY redirect_root;
    wchar_t *redirect_name;
    LSTATUS err;

    redirect_name = NULL;

    EnterCriticalSection(&reg_hook_lock);
    err = reg_hook_resolve_redirect_locked(
            handle,
            subkey,
            &redirect_root,
            &redirect_name);

    if (err != ERROR_SUCCESS || redirect_name != NULL) {
        LeaveCriticalSection(&reg_hook_lock);

        if (err != ERROR_SUCCESS) {
            free(redirect_name);

            return err;
        }

        err = next_RegGetValueW(
                redirect_root,
                redirect_name,
                name,
                flags,
                type,
                pData,
                numData);
        free(redirect_name);

        return err;
    }

    if (subkey == NULL) {
        key = reg_hook_match_key_locked(handle);
    } else {
        hook_RegOpenKeyExW(handle, subkey, flags, 1, &tmp);
        key = reg_hook_match_key_locked(tmp);
    }

    if (key == NULL) {
        LeaveCriticalSection(&reg_hook_lock);

        if (tmp != NULL) {
            hook_RegCloseKey(tmp);
        }

        return next_RegGetValueW(
                handle,
                subkey,
                name,
                flags,
                type,
                pData,
                numData);
    }

    val = reg_hook_match_val_locked(key, name);

    if (val != NULL && val->read != NULL) {
        val->read(pData, numData);
        err = ERROR_SUCCESS;
    } else {
        err = ERROR_NOT_FOUND;
    }

    LeaveCriticalSection(&reg_hook_lock);

    if (tmp != NULL) {
        hook_RegCloseKey(tmp);
    }

    return err;
}

static LSTATUS WINAPI hook_RegGetValueA(
    HKEY    handle,
    LPCSTR  subkey,
    LPCSTR  name,
    uint32_t flags,
    uint32_t *type,
    void   *pData,
    uint32_t *numData)
{
    char *redirect_name;
    HKEY redirect_root;
    LSTATUS err;

    if (InterlockedCompareExchange(&reg_hook_redirect_active, 0, 0) == 0) {
        return next_RegGetValueA(
                handle,
                subkey,
                name,
                flags,
                type,
                pData,
                numData);
    }

    err = reg_hook_resolve_redirect_a(
            handle, subkey, &redirect_root, &redirect_name);

    if (err != ERROR_SUCCESS) {
        return err;
    }

    if (redirect_name != NULL) {
        handle = redirect_root;
        subkey = redirect_name;
    }

    err = next_RegGetValueA(
            handle,
            subkey,
            name,
            flags,
            type,
            pData,
            numData);
    free(redirect_name);

    return err;
}

static LSTATUS WINAPI hook_RegQueryInfoKeyW(
    HKEY      hKey,
    LPWSTR    lpClass,
    LPDWORD   lpcchClass,
    LPDWORD   lpReserved,
    LPDWORD   lpcSubKeys,
    LPDWORD   lpcbMaxSubKeyLen,
    LPDWORD   lpcbMaxClassLen,
    LPDWORD   lpcValues,
    LPDWORD   lpcbMaxValueNameLen,
    LPDWORD   lpcbMaxValueLen,
    LPDWORD   lpcbSecurityDescriptor,
    PFILETIME lpftLastWriteTime)
{
    struct reg_hook_key *key;
    LSTATUS err;

    EnterCriticalSection(&reg_hook_lock);

    key = reg_hook_match_key_locked(hKey);

    /* Check if this is a virtualized registry key */

    if (key == NULL) {
        LeaveCriticalSection(&reg_hook_lock);

        return next_RegQueryInfoKeyW(
            hKey,
            lpClass,
            lpcchClass,
            lpReserved,
            lpcSubKeys,
            lpcbMaxSubKeyLen,
            lpcbMaxClassLen,
            lpcValues,
            lpcbMaxValueNameLen,
            lpcbMaxValueLen,
            lpcbSecurityDescriptor,
            lpftLastWriteTime);
    }

    // This is the only one I've seen even be changed, so it's all I'm doing
    // until I see otherwise.
    *lpcValues = key->nvals;
    LeaveCriticalSection(&reg_hook_lock);
    return ERROR_SUCCESS;
}

static LSTATUS WINAPI hook_RegEnumValueW(
    HKEY    hkey,
    DWORD   dwIndex,
    LPWSTR   lpValueName,
    LPDWORD lpcchValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE  lpData,
    LPDWORD lpcbData)
{
    struct reg_hook_key *key;
    HRESULT hr;
    LSTATUS err;

    EnterCriticalSection(&reg_hook_lock);

    key = reg_hook_match_key_locked(hkey);

    /* Check if this is a virtualized registry key */

    if (key == NULL) {
        LeaveCriticalSection(&reg_hook_lock);

        return next_RegEnumValueW(
            hkey,
            dwIndex,
            lpValueName,
            lpcchValueName,
            lpReserved,
            lpType,
            lpData,
            lpcbData);
    }

    if (dwIndex >= key->nvals) {
        LeaveCriticalSection(&reg_hook_lock);
        return ERROR_NO_MORE_ITEMS; // Pretty sure this is what it actually returns here?
    }

    wcscpy_s(lpValueName, *lpcchValueName, key->vals[dwIndex].name);
    *lpcchValueName = wcslen(key->vals[dwIndex].name);
    LeaveCriticalSection(&reg_hook_lock);
    return ERROR_SUCCESS;
}

HRESULT reg_hook_read_bin(
        void *bytes,
        uint32_t *nbytes,
        const void *src_bytes,
        size_t src_nbytes)
{
    assert(src_bytes != NULL || src_nbytes == 0);

    if (bytes != NULL) {
        if (nbytes == NULL || *nbytes < src_nbytes) {
            return HRESULT_FROM_WIN32(ERROR_MORE_DATA);
        }

        memcpy(bytes, src_bytes, src_nbytes);
    }

    if (nbytes != NULL) {
        *nbytes = src_nbytes;
    }

    return S_OK;
}

HRESULT reg_hook_read_u32(
        void *bytes,
        uint32_t *nbytes,
        uint32_t src)
{
    if (bytes != NULL) {
        if (nbytes == NULL || *nbytes < sizeof(uint32_t)) {
            return HRESULT_FROM_WIN32(ERROR_MORE_DATA);
        }

        memcpy(bytes, &src, sizeof(uint32_t));
    }

    if (nbytes != NULL) {
        *nbytes = sizeof(uint32_t);
    }

    return S_OK;
}

HRESULT reg_hook_read_wstr(
        void *bytes,
        uint32_t *nbytes,
        const wchar_t *src)
{
    size_t src_nbytes;

    assert(src != NULL);

    src_nbytes = (wcslen(src) + 1) * sizeof(wchar_t);

    if (bytes != NULL) {
        if (nbytes == NULL || *nbytes < src_nbytes) {
            return HRESULT_FROM_WIN32(ERROR_MORE_DATA);
        }

        memcpy(bytes, src, src_nbytes);
    }

    if (nbytes != NULL) {
        *nbytes = src_nbytes;
    }

    return S_OK;
}
