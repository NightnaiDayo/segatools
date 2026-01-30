#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "hook/table.h"

#include "hooklib/dll.h"

#include "util/dprintf.h"

#include "soul-gfx.h"

static PVOID WINAPI hook_wglGetProcAddress(LPCSTR unnamedParam1);
static PVOID (WINAPI *next_wglGetProcAddress)(LPCSTR unnamedParam1);

static const struct hook_symbol soulgfx_syms[] = {
    {
        .name = "wglGetProcAddress",
        .patch = hook_wglGetProcAddress,
        .link = (void **) &next_wglGetProcAddress,
    }
};

void soulgfx_hook_init()
{
    dprintf("GFX: Hooks enabled.\n");

    hook_table_apply(
        NULL,
        "opengl32.dll",
        soulgfx_syms,
        _countof(soulgfx_syms));
}

void dummy() {

}

static PVOID WINAPI hook_wglGetProcAddress(LPCSTR unnamedParam1) {
    PVOID result = next_wglGetProcAddress(unnamedParam1);

    // Temporary hack just to get list of missing functions
    if (!result) {
        dprintf("GFX: %s not found, replacing with stub\n", unnamedParam1);
        result = (PVOID)dummy;
    }

    return result;
}
