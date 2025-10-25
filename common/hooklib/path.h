#pragma once

#include <windows.h>

#include <stdbool.h>
#include <stddef.h>

typedef HRESULT (*path_hook_t)(
        const wchar_t *src,
        wchar_t *dest,
        size_t *count);

HRESULT path_hook_push(path_hook_t hook);
void path_hook_insert_hooks(HMODULE target);
int path_compare_w(const wchar_t *string1, const wchar_t *string2, size_t count);
BOOL path_transform_a(char **out, const char *src);
BOOL path_transform_w(wchar_t **out, const wchar_t *src);
BOOL path_transform_args_a(const char* str, char delimiter, char* buf, size_t size);
BOOL path_transform_args_w(const wchar_t* str, wchar_t delimiter, wchar_t* buf, size_t size);

static inline bool path_is_separator_w(wchar_t c)
{
    return c == L'\\' || c == L'/';
}
