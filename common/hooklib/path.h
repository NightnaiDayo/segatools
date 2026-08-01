#pragma once

#include <windows.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __MINGW32__
// MinGW already has this definition
#include <ntdef.h> 

#else
// This is only defined in the Windows Driver SDK, so we copy the definition in here
typedef struct _REPARSE_DATA_BUFFER
{
    ULONG  ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union
    {
        struct
        {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG  Flags;
            WCHAR  PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct
        {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            WCHAR  PathBuffer[1];
        } MountPointReparseBuffer;
        struct
        {
            UCHAR  DataBuffer[1];
        } GenericReparseBuffer;
    };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;
#endif

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
