#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hook/table.h"
#include "util/dprintf.h"

static HCURSOR my_SetCursor(HCURSOR hCursor);
static HCURSOR (*next_SetCursor)(HCURSOR hCursor);
static BOOL my_SetCursorPos(int x, int y);
static BOOL my_SetPhysicalCursorPos(int x, int y);
static int my_ShowCursor(BOOL bShow);

static const struct hook_symbol cursor_syms[] = {
    {
        .name  = "SetCursor",
        .patch = my_SetCursor,
        .link  = (void **) &next_SetCursor
    },/*{
        .name  = "SetCursorPos",
        .patch = my_SetCursorPos,
    },*/ {
        .name  = "SetPhysicalCursorPos",
        .patch = my_SetPhysicalCursorPos
    }, /*{
        .name  = "ShowCursor",
        .patch = my_ShowCursor
    }*/
};

void cursor_hook_init()
{
    hook_table_apply(
        NULL,
        "user32.dll",
        cursor_syms,
        _countof(cursor_syms));
    
    dprintf("Cursor: Init\n");
}

static BOOL my_SetCursorPos(int x, int y)
{
    dprintf("my_SetCursorPos Hit! x %d y %d\n", x, y);
    return true;
}

static BOOL my_SetPhysicalCursorPos(int x, int y)
{
    dprintf("my_SetPhysicalCursorPos Hit! x %d y %d\n", x, y);
    return true;
}

static int my_ShowCursor(BOOL bShow)
{
    dprintf("my_ShowCursor Hit!\n");
    return 0;
}

static HCURSOR my_SetCursor(HCURSOR hCursor)
{
    dprintf("my_SetCursor Hit!\n");
    HCURSOR fake_cursor;

    if ( hCursor )
        return next_SetCursor(hCursor);
    fake_cursor = LoadCursorA(0, (LPCSTR)0x7F00);
    next_SetCursor(fake_cursor);
    return 0;
}
