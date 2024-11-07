#include <windows.h>

#include <assert.h>
#include <stdlib.h>

#include "gfxhook/gfx.h"

#include "hook/table.h"

#include "util/dprintf.h"

/* Hook functions */

static BOOL WINAPI hook_ShowWindow(HWND hWnd, int nCmdShow);
static BOOL WINAPI hook_CreateWindowExA(
    DWORD dwExStyle,
    LPCSTR lpClassName,
    LPCSTR lpWindowName,
    DWORD dwStyle,
    int X,
    int Y,
    int nWidth,
    int nHeight,
    HWND hWndParent,
    HMENU hMenu,
    HINSTANCE hInstance,
    LPVOID lpParam
);

/* Link pointers */

static BOOL (WINAPI *next_ShowWindow)(HWND hWnd, int nCmdShow);
static BOOL (WINAPI *next_CreateWindowExA)(
    DWORD dwExStyle,
    LPCSTR lpClassName,
    LPCSTR lpWindowName,
    DWORD dwStyle,
    int X,
    int Y,
    int nWidth,
    int nHeight,
    HWND hWndParent,
    HMENU hMenu,
    HINSTANCE hInstance,
    LPVOID lpParam
);

static struct gfx_config gfx_config;

static const struct hook_symbol gfx_hooks[] = {
    {
        .name = "ShowWindow",
        .patch = hook_ShowWindow,
        .link = (void **) &next_ShowWindow,
    }, {
        .name = "CreateWindowExA",
        .patch = hook_CreateWindowExA,
        .link = (void **) &next_CreateWindowExA,
    },
};

void gfx_hook_init(const struct gfx_config *cfg)
{
    assert(cfg != NULL);

    if (!cfg->enable) {
        return;
    }

    memcpy(&gfx_config, cfg, sizeof(*cfg));
    hook_table_apply(NULL, "user32.dll", gfx_hooks, _countof(gfx_hooks));
}

static BOOL WINAPI hook_ShowWindow(HWND hWnd, int nCmdShow)
{
    dprintf("Gfx: ShowWindow hook hit\n");

    if (!gfx_config.framed && nCmdShow == SW_RESTORE) {
        nCmdShow = SW_SHOW;
    }

    return next_ShowWindow(hWnd, nCmdShow);
}

static BOOL WINAPI hook_CreateWindowExA(
    DWORD dwExStyle,
    LPCSTR lpClassName,
    LPCSTR lpWindowName,
    DWORD dwStyle,
    int X,
    int Y,
    int nWidth,
    int nHeight,
    HWND hWndParent,
    HMENU hMenu,
    HINSTANCE hInstance,
    LPVOID lpParam
)
{
    dprintf("Gfx: CreateWindowExA hook hit\n");

    // Set to WS_OVERLAPPEDWINDOW to enable a window with a border and windowed style
    if (gfx_config.windowed) {
        dwStyle = WS_OVERLAPPEDWINDOW;

        if (!gfx_config.framed) {
            dwStyle = WS_POPUP;
        }
    }

    return next_CreateWindowExA(
        dwExStyle,
        lpClassName,
        lpWindowName,
        dwStyle,
        X,
        Y,
        nWidth,
        nHeight,
        hWndParent,
        hMenu,
        hInstance,
        lpParam
    );
}
