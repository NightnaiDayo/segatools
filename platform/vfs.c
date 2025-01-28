#include <windows.h>
#include <shlwapi.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "hooklib/path.h"
#include "hooklib/reg.h"

#include "hook/procaddr.h"
#include "hook/table.h"

#include "platform/vfs.h"

#include "util/dprintf.h"

static void vfs_fixup_path(wchar_t *path, size_t max_count);
static HRESULT vfs_mkdir_rec(const wchar_t *path);
static HRESULT vfs_path_hook(const wchar_t *src, wchar_t *dest, size_t *count);
static HRESULT vfs_path_hook_nthome(
        const wchar_t *src,
        wchar_t *dest,
        size_t *count);
static HRESULT vfs_path_hook_w10home(
        const wchar_t *src,
        wchar_t *dest,
        size_t *count);
static HRESULT vfs_path_hook_option(
        const wchar_t *src,
        wchar_t *dest,
        size_t *count);
static HRESULT vfs_reg_read_amfs(void *bytes, uint32_t *nbytes);
static HRESULT vfs_reg_read_appdata(void *bytes, uint32_t *nbytes);

static wchar_t* hook_System_getAppRootPath();
static wchar_t* (*next_System_getAppRootPath)();

static wchar_t* hook_AppImage_getOptionMountRootPath();
static wchar_t* (*next_AppImage_getOptionMountRootPath)();

static const struct hook_symbol amdaemon_syms[] = {
    {
        .name = "System_getAppRootPath",
        .patch = hook_System_getAppRootPath,
        .link = (void **) &next_System_getAppRootPath,
    },
    {
        .name = "AppImage_getOptionMountRootPath",
        .patch = hook_AppImage_getOptionMountRootPath,
        .link = (void **) &next_AppImage_getOptionMountRootPath,
    },
};

static wchar_t game[5] = {0};
static wchar_t vfs_nthome_real[MAX_PATH];
static const wchar_t vfs_nthome[] = L"C:\\Documents and Settings\\AppUser";
static const size_t vfs_nthome_len = _countof(vfs_nthome) - 1;

static const wchar_t vfs_w10home[] = L"C:\\Users\\AppUser";
static const size_t vfs_w10home_len = _countof(vfs_w10home) - 1;

static const wchar_t vfs_option[] = L"C:\\Mount\\Option";
static const size_t vfs_option_len = _countof(vfs_option) - 1;

static const wchar_t vfs_amfs[] = L"D:\\amfs\\";
static const size_t vfs_amfs_len = _countof(vfs_amfs) - 1;

static wchar_t vfs_appdata[MAX_PATH];
static size_t vfs_appdata_len;

static bool use_alls_vfs;

static const struct reg_hook_val vfs_reg_vals[] = {
    {
        .name   = L"AMFS",
        .read   = vfs_reg_read_amfs,
        .type   = REG_SZ,
    }, {
        .name   = L"APPDATA",
        .read   = vfs_reg_read_appdata,
        .type   = REG_SZ
    },
};

static struct vfs_config vfs_config;

static HRESULT get_appdata_hook_path() {
    // At most cases, the hardcoded path is Y:\game_id
    int ret = swprintf_s(vfs_appdata, MAX_PATH, L"Y:\\%ls\\", game);
    if (ret < 0) {
        return E_FAIL;
    }
    vfs_appdata_len = wcslen(vfs_appdata);
    return S_OK;
}

HRESULT vfs_hook_init(const struct vfs_config *config, const char* game_id, const char* keychip_game_id, bool is_alls)
{
    wchar_t temp[MAX_PATH];
    size_t nthome_len;
    DWORD home_ok;
    HRESULT hr;

    assert(config != NULL);

    if (!config->enable) {
        return S_FALSE;
    }

    if (config->amfs[0] == L'\0') {
        dprintf("Vfs: FATAL: AMFS path not specified in INI file\n");

        return E_FAIL;
    }

    if (config->appdata[0] == L'\0') {
        dprintf("Vfs: FATAL: APPDATA path not specified in INI file\n");

        return E_FAIL;
    }

    if (config->option[0] == L'\0') {
        dprintf("Vfs: WARNING: OPTION path not specified in INI file\n");
    } else if (!PathFileExistsW(config->option)) {
        dprintf("Vfs: FATAL: OPTION path does not exist\n");
        dprintf("            Configured: \"%ls\"\n", config->option);
        GetFullPathNameW(config->option, _countof(temp), temp, NULL);
        dprintf("            Expanded:   \"%ls\"\n", temp);

        return E_FAIL;
    } else if (!(GetFileAttributesW(config->option) & FILE_ATTRIBUTE_DIRECTORY)) {
        dprintf("Vfs: FATAL: OPTION path doesn't point to a directory\n");
        dprintf("            Configured: \"%ls\"\n", config->option);
        GetFullPathNameW(config->option, _countof(temp), temp, NULL);
        dprintf("            Expanded:   \"%ls\"\n", temp);

        return E_FAIL;
    }

    use_alls_vfs = is_alls;

    if (game_id && game_id[0] != '\0') {
        mbstowcs(game, game_id, 4);
    }

    // Override with the manually provided game id if specified in config
    if (keychip_game_id && keychip_game_id[0] != '\0') {
        mbstowcs(game, keychip_game_id, 4);
    }

    if (game[0] == L'\0') {
        dprintf("Vfs: FATAL: No game id specified\n");
        return E_FAIL;
    }

    // Construct the appdata path from the game id and the Y: drive
    hr = get_appdata_hook_path();
    if (FAILED(hr)) {
        dprintf("Vfs: FATAL: Failed to construct APPDATA path from game id\n");
        return hr;
    }

    home_ok = GetEnvironmentVariableW(
            L"USERPROFILE",
            vfs_nthome_real,
            _countof(vfs_nthome_real));

    if (!home_ok) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        dprintf("Vfs: Failed to query %%USERPROFILE%% env var: %x\n",
                (int) hr);

        return hr;
    }

    memcpy(&vfs_config, config, sizeof(*config));

    vfs_fixup_path(vfs_nthome_real, _countof(vfs_nthome_real));
    vfs_fixup_path(vfs_config.amfs, _countof(vfs_config.amfs));
    vfs_fixup_path(vfs_config.appdata, _countof(vfs_config.appdata));

    if (vfs_config.option[0] != L'\0') {
        vfs_fixup_path(vfs_config.option, _countof(vfs_config.option));
    }

    hr = vfs_mkdir_rec(vfs_config.amfs);

    if (FAILED(hr)) {
        dprintf("Vfs: Failed to create AMFS dir %S: %x\n",
                config->amfs,
                (int) hr);
    }

    hr = vfs_mkdir_rec(vfs_config.appdata);

    if (FAILED(hr)) {
        dprintf("Vfs: Failed to create APPDATA dir %S: %x\n",
                config->appdata,
                (int) hr);

        dprintf("Vfs: NOTE: SEGA Y: drive APPDATA, not Windows %%APPDATA%%.\n");
    }

    /* Need to create the temp subdirectory, not just nthome itself */

    nthome_len = wcslen(vfs_nthome_real);
    wcscpy_s(temp, _countof(temp), vfs_nthome_real);
    wcscpy_s(temp + nthome_len, _countof(temp) - nthome_len, L"temp");

    hr = vfs_mkdir_rec(temp);

    if (FAILED(hr)) {
        dprintf("Vfs: Failed to create %S: %x\n", temp, (int) hr);
    }

    /* Not auto-creating option directory as it is normally a read-only mount */

    hr = path_hook_push(vfs_path_hook);

    if (FAILED(hr)) {
        return hr;
    }

    hr = path_hook_push(vfs_path_hook_nthome);

    if (FAILED(hr)) {
        return hr;
    }

    hr = path_hook_push(vfs_path_hook_w10home);

    if (FAILED(hr)) {
        return hr;
    }

    if (vfs_config.option[0] != L'\0') {
        hr = path_hook_push(vfs_path_hook_option);

        if (FAILED(hr)) {
            return hr;
        }
    }

    hr = reg_hook_push_key(
            HKEY_LOCAL_MACHINE,
            L"SYSTEM\\SEGA\\SystemProperty\\mount",
            vfs_reg_vals,
            _countof(vfs_reg_vals));

    if (FAILED(hr)) {
        return hr;
    }

    proc_addr_table_push(
        NULL,
        "amdaemon_api.dll",
        amdaemon_syms,
        _countof(amdaemon_syms)
    );

    return S_OK;
}

static void vfs_fixup_path(wchar_t *path, size_t max_count)
{
    size_t count;
    wchar_t abspath[MAX_PATH];

    assert(path != NULL);
    /* Requirement for PathIsRelativeW */
    assert(max_count <= MAX_PATH);

    if (PathIsRelativeW(path)) {
        count = GetFullPathNameW(path, _countof(abspath), abspath, NULL);

        /* GetFullPathName's length return value is tricky, because it includes
           the NUL terminator on failure, but doesn't on success.
           Check if it fits the temp buf (else it's a failure and includes NUL
           anyway), then if it fits the target buf, NUL included. */
        if (count == 0 || count > _countof(abspath) || count >= max_count) {
            goto fail;
        }

        wcscpy_s(path, max_count, abspath);
    } else {
        count = wcslen(path);
    }

    if (path_is_separator_w(path[count - 1])) {
        return;
    }

    if (count + 2 > max_count) {
        goto fail;
    }

    path[count + 0] = L'\\';
    path[count + 1] = L'\0';
    return;

fail:
    dprintf("Vfs: FATAL: Path too long: %S\n", path);
    abort();
}

static HRESULT vfs_mkdir_rec(const wchar_t *path)
{
    wchar_t *copy;
    wchar_t *pos;
    wchar_t wc;
    HRESULT hr;
    DWORD attr;
    BOOL ok;

    assert(path != NULL);

    copy = _wcsdup(path);

    if (copy == NULL) {
        hr = HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);

        goto end;
    }

    pos = copy;

    do {
        wc = *pos;

        if (wc == L'\0' || wc == L'/' || wc == L'\\') {
            *pos = L'\0';
            attr = GetFileAttributesW(copy);

            if (attr == INVALID_FILE_ATTRIBUTES) {
                ok = CreateDirectoryW(copy, NULL);

                if (!ok) {
                    hr = HRESULT_FROM_WIN32(GetLastError());

                    goto end;
                }
            }

            *pos = wc;
        }

        pos++;
    } while (wc != L'\0');

    hr = S_OK;

end:
    free(copy);

    return hr;
}

static HRESULT vfs_path_hook(const wchar_t *src, wchar_t *dest, size_t *count)
{
    const wchar_t *redir;
    size_t required;
    size_t redir_len;
    size_t shift;
    size_t src_len;

    assert(src != NULL);
    assert(count != NULL);

    if (src[0] == L'\0' || src[1] != L':') {
        return S_FALSE;
    }

    if (use_alls_vfs)
    {
        if (path_compare_w(src, vfs_amfs, vfs_amfs_len) == 0) {
            shift = path_is_separator_w(src[vfs_amfs_len]) ? 1 : 0;
            redir = vfs_config.amfs;
            redir_len = wcslen(redir);
            src_len = vfs_amfs_len;
            required = wcslen(src) - vfs_amfs_len - shift + redir_len + 1;
        }
        // Sometimes the game uses the appdata path provided appended with the game id, we also need to redirect that
        else if (wcsncmp(src, vfs_appdata, vfs_appdata_len) == 0 && wcsstr(src + vfs_appdata_len, game) == src + vfs_appdata_len) {
            shift = path_is_separator_w(src[vfs_appdata_len + wcslen(game)]) ? 1 : 0;
            redir = vfs_config.appdata;
            redir_len = wcslen(redir);
            src_len = vfs_appdata_len + wcslen(game);
            required = wcslen(src) - src_len - shift + redir_len + 1;
        }
        // This one is the normal appdata path returned to the game
        else if (path_compare_w(src, vfs_appdata, vfs_appdata_len) == 0) {
            shift = path_is_separator_w(src[vfs_appdata_len]) ? 1 : 0;
            redir = vfs_config.appdata;
            redir_len = wcslen(redir);
            src_len = vfs_appdata_len;
            required = wcslen(src) - vfs_appdata_len - shift + redir_len + 1;
        }
        else
        {
            return S_FALSE;
        }
    }
    else
    {
        switch (src[0]) {
            case L'e':
            case L'E':
                redir = vfs_config.amfs;

                break;

            case L'y':
            case L'Y':
                redir = vfs_config.appdata;

                break;

            default:
                return S_FALSE;
        }

        shift = path_is_separator_w(src[2]) ? 1 : 0;
        src_len = 2;

        /* Cut off <prefix>\, replace with redir path, count NUL terminator */

        redir_len = wcslen(redir);
        required = wcslen(src) - src_len - shift + redir_len + 1;
    }

    if (dest != NULL) {
        if (required > *count) {
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        }

        wcscpy_s(dest, *count, redir);
        wcscpy_s(dest + redir_len, *count - redir_len, src + src_len + shift);
    }

    *count = required;

    return S_OK;
}

static HRESULT vfs_path_hook_nthome(
        const wchar_t *src,
        wchar_t *dest,
        size_t *count)
{
    size_t required;
    size_t redir_len;
    size_t shift;

    assert(src != NULL);
    assert(count != NULL);

    /* Case-insensitive check to see if src starts with vfs_nthome */

    if (path_compare_w(src, vfs_nthome, vfs_nthome_len) != 0) {
        return S_FALSE;
    }

    /* Check if the character after vfs_nthome is a separator or the end of
       the string */

    if (!path_is_separator_w(src[vfs_nthome_len]) &&
            src[vfs_nthome_len] != L'\0')
    {
        return S_FALSE;
    }

    /* Cut off the matched <prefix>\, add the replaced prefix, count NUL */

    shift = path_is_separator_w(src[vfs_nthome_len]) ? 1 : 0;
    redir_len = wcslen(vfs_nthome_real);
    required = wcslen(src) - vfs_nthome_len - shift + redir_len + 1;

    if (dest != NULL) {
        if (required > *count) {
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        }

        wcscpy_s(dest, *count, vfs_nthome_real);
        wcscpy_s(dest + redir_len, *count - redir_len, src + vfs_nthome_len + shift);
    }

    *count = required;

    return S_OK;
}

static HRESULT vfs_path_hook_w10home(
        const wchar_t *src,
        wchar_t *dest,
        size_t *count)
{
    size_t required;
    size_t redir_len;
    size_t shift;

    assert(src != NULL);
    assert(count != NULL);

    /* Case-insensitive check to see if src starts with vfs_w10home */

    if (path_compare_w(src, vfs_w10home, vfs_w10home_len) != 0) {
        return S_FALSE;
    }

    /* Check if the character after vfs_w10home is a separator or the end of
       the string */

    if (!path_is_separator_w(src[vfs_w10home_len]) &&
            src[vfs_w10home_len] != L'\0')
    {
        return S_FALSE;
    }

    /* Cut off the matched <prefix>\, add the replaced prefix, count NUL */

    shift = path_is_separator_w(src[vfs_w10home_len]) ? 1 : 0;
    redir_len = wcslen(vfs_nthome_real);
    required = wcslen(src) - vfs_w10home_len - shift + redir_len + 1;

    if (dest != NULL) {
        if (required > *count) {
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        }

        wcscpy_s(dest, *count, vfs_nthome_real);
        wcscpy_s(dest + redir_len, *count - redir_len, src + vfs_w10home_len + shift);
    }

    *count = required;

    return S_OK;
}

static HRESULT vfs_path_hook_option(
        const wchar_t *src,
        wchar_t *dest,
        size_t *count)
{
    size_t required;
    size_t redir_len;
    size_t shift;

    assert(src != NULL);
    assert(count != NULL);

    /* Case-insensitive check to see if src starts with vfs_option */

    if (path_compare_w(src, vfs_option, vfs_option_len) != 0) {
        return S_FALSE;
    }

    /* Check if the character after vfs_nthome is a separator or the end of
       the string */

    if (!path_is_separator_w(src[vfs_option_len]) &&
            src[vfs_option_len] != L'\0')
    {
        return S_FALSE;
    }

    /* Cut off the matched <prefix>\, add the replaced prefix, count NUL */

    shift = path_is_separator_w(src[vfs_option_len]) ? 1 : 0;
    redir_len = wcslen(vfs_config.option);
    required = wcslen(src) - vfs_option_len - shift + redir_len + 1;

    if (dest != NULL) {
        if (required > *count) {
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        }

        wcscpy_s(dest, *count, vfs_config.option);
        wcscpy_s(dest + redir_len, *count - redir_len, src + vfs_option_len + shift);
    }

    *count = required;

    return S_OK;
}

static HRESULT vfs_reg_read_amfs(void *bytes, uint32_t *nbytes)
{
    return reg_hook_read_wstr(bytes, nbytes, vfs_amfs);
}

static HRESULT vfs_reg_read_appdata(void *bytes, uint32_t *nbytes)
{
    return reg_hook_read_wstr(bytes, nbytes, vfs_appdata);
}

static wchar_t* hook_System_getAppRootPath()
{
    wchar_t *path = malloc(sizeof(wchar_t) * MAX_PATH);
    wcscpy_s(path, MAX_PATH, vfs_config.appdata);
    wcscat_s(path, MAX_PATH, game);
    wcscat_s(path, MAX_PATH, L"\\");

    return path;
}

static wchar_t* hook_AppImage_getOptionMountRootPath()
{
    wchar_t *path = malloc(sizeof(wchar_t) * MAX_PATH);
    wcscpy_s(path, MAX_PATH, vfs_config.option);

    return path;
}
