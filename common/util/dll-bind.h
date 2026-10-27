#pragma once

#include <windows.h>

#include <stdlib.h>
#include <stdbool.h>

struct dll_bind_sym {
    /* Symbol name to locate in the source DLL */
    const char *sym;

    /* Offset where the symbol pointer should be written in the target
       structure */
    ptrdiff_t off;
};

/*
    Bind a list of DLL symbols into a structure that contains function
    pointers.

    - dest: Pointer to destination structure.
    - src: Handle to source DLL.
    - src: Handle to the hook DLL itself.
    - syms_pos: Pointer to a (mutable) pointer which points to the start of an
        (immutable) table of symbols and structure offsets. This mutable
        pointer is advanced until either a symbol fails to bind, in which case
        an error is returned if optional is set to false, or the end of
        the table is reached, in which case success is returned.
   - syms_count: Number of entries in the symbol table.
   - optional: false to make binding fail if a symbol is missing, true to
       ignore missing symbols and leave the default.
*/
HRESULT dll_bind(
        void *dest,
        HINSTANCE src,
        HINSTANCE self,
        const struct dll_bind_sym **syms_pos,
        size_t syms_count,
        bool optional);
