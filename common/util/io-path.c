#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "util/dprintf.h"
#include "util/io-path.h"

static bool io_path_expand(
        wchar_t *out,
        size_t out_count,
        const wchar_t *path,
        const wchar_t *source);
static bool io_path_from_root(
        wchar_t *out,
        size_t out_count,
        const wchar_t *root_filename);
static bool io_path_is_separator(wchar_t ch);

bool io_path_config_load(
        wchar_t *out,
        size_t out_count,
        const wchar_t *section,
        const wchar_t *key,
        const wchar_t *env_name,
        const wchar_t *root_filename,
        const wchar_t *filename)
{
    wchar_t path[MAX_PATH];
    DWORD count;

    assert(out != NULL);
    assert(out_count > 0);
    assert(section != NULL);
    assert(key != NULL);
    assert(filename != NULL);

    out[0] = L'\0';

    count = GetPrivateProfileStringW(
            section,
            key,
            L"",
            path,
            _countof(path),
            filename);

    if (count >= _countof(path) - 1 && path[0] != L'\0') {
        dprintf("IO path: INI value [%S] %S may be truncated "
                "(exceeds %d characters)\n",
                section,
                key,
                (int) _countof(path) - 1);
    }

    if (path[0] != L'\0') {
        return io_path_expand(out, out_count, path, key);
    }

    if (env_name != NULL) {
        count = GetEnvironmentVariableW(env_name, path, _countof(path));

        if (count >= _countof(path)) {
            dprintf("IO path: Environment variable too long: %S\n", env_name);

            return false;
        }

        if (count != 0 && path[0] != L'\0') {
            return io_path_expand(out, out_count, path, env_name);
        }
    }

    return io_path_from_root(out, out_count, root_filename);
}

static bool io_path_expand(
        wchar_t *out,
        size_t out_count,
        const wchar_t *path,
        const wchar_t *source)
{
    DWORD expanded;

    assert(out != NULL);
    assert(out_count > 0);
    assert(path != NULL);
    assert(source != NULL);

    expanded = ExpandEnvironmentStringsW(path, out, (DWORD) out_count);

    if (expanded == 0) {
        dprintf("IO path: Failed to expand path from %S: %lx\n",
                source,
                GetLastError());
        out[0] = L'\0';

        return false;
    }

    if (expanded > out_count) {
        dprintf("IO path: Expanded path from %S is too long: %S\n",
                source,
                path);
        out[0] = L'\0';

        return false;
    }

    return true;
}

static bool io_path_from_root(
        wchar_t *out,
        size_t out_count,
        const wchar_t *root_filename)
{
    wchar_t root[MAX_PATH];
    wchar_t expanded_root[MAX_PATH];
    size_t root_len;
    size_t file_len;
    DWORD count;

    assert(out != NULL);
    assert(out_count > 0);

    if (root_filename == NULL || root_filename[0] == L'\0') {
        return false;
    }

    count = GetEnvironmentVariableW(
            L"SEGATOOLS_IO_ROOT",
            root,
            _countof(root));

    if (count >= _countof(root)) {
        dprintf("IO path: SEGATOOLS_IO_ROOT is too long\n");

        return false;
    }

    if (count == 0 || root[0] == L'\0') {
        return false;
    }

    if (!io_path_expand(
            expanded_root,
            _countof(expanded_root),
            root,
            L"SEGATOOLS_IO_ROOT")) {
        return false;
    }

    root_len = wcslen(expanded_root);
    file_len = wcslen(root_filename);

    if (root_len == 0) {
        return false;
    }

    if (root_len + file_len + 2 > out_count) {
        dprintf("IO path: SEGATOOLS_IO_ROOT path is too long: %S\\%S\n",
                expanded_root,
                root_filename);

        return false;
    }

    wcscpy_s(out, out_count, expanded_root);

    if (!io_path_is_separator(out[root_len - 1])) {
        out[root_len++] = L'\\';
        out[root_len] = L'\0';
    }

    wcscat_s(out, out_count, root_filename);

    /* The SEGATOOLS_IO_ROOT fallback is broadcast to every hook at once, so a
       single global setting is shared by games that may not each ship an IO
       DLL in that directory. Unlike an explicit INI/env path (direct user
       intent, allowed to fail loudly at load time), a synthesized root path
       that does not exist should quietly fall back to the built-in IO
       implementation instead of aborting hook startup. */
    if (GetFileAttributesW(out) == INVALID_FILE_ATTRIBUTES) {
        dprintf("IO path: SEGATOOLS_IO_ROOT DLL not found, "
                "using built-in IO: %S\n",
                out);
        out[0] = L'\0';

        return false;
    }

    return true;
}

static bool io_path_is_separator(wchar_t ch)
{
    return ch == L'\\' || ch == L'/';
}
