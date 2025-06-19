/*
This part (touch screen hook) is mostly taken from spicetools, which is GPL.
This means there can be some license issues if you do use this code in some other places without including source code.
*/
#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "hook/com-proxy.h"
#include "hook/table.h"

#include "hooklib/config.h"
#include "hooklib/dll.h"
#include "hooklib/touch.h"

#include "util/dprintf.h"

/* API hooks */

static LRESULT hook_wndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

static ATOM WINAPI hook_RegisterClassExA(
    WNDCLASSEXA* wndClass
);

static int WINAPI hook_GetSystemMetrics(
    int nIndex
);

static BOOL WINAPI hook_RegisterTouchWindow(
    HWND hwnd,
    ULONG ulFlags
);

static BOOL WINAPI hook_GetTouchInputInfo(
    HANDLE hTouchInput,
    UINT cInputs,
    PTOUCHINPUT pInputs,
    int cbSize
);

static HCURSOR WINAPI hook_SetCursor(HCURSOR cursor);

/* Link pointers */

static ATOM (WINAPI *next_RegisterClassExA)(
    const WNDCLASSEXA* wndClass
);

static int (WINAPI *next_GetSystemMetrics)(
    int nIndex
);

static BOOL (WINAPI *next_RegisterTouchWindow)(
    HWND hwnd,
    ULONG ulFlags
);

static BOOL (WINAPI *next_GetTouchInputInfo)(
    HANDLE hTouchInput,
    UINT cInputs,
    PTOUCHINPUT pInputs,
    int cbSize
);

static HCURSOR(WINAPI *next_SetCursor)(HCURSOR cursor);

static bool touch_hook_initted;
static bool touch_held;
static HWND registered_hWnd;
static struct touch_screen_config touch_config;
static WNDPROC orig_wndProc;
static HCURSOR defaultCursor;

static const struct hook_symbol touch_hooks[] = {
    {
        .name   = "RegisterClassExA",
        .patch  = hook_RegisterClassExA,
        .link   = (void**)&next_RegisterClassExA
    },
    {
        .name   = "GetSystemMetrics",
        .patch  = hook_GetSystemMetrics,
        .link   = (void **) &next_GetSystemMetrics
    },
    {
        .name   = "RegisterTouchWindow",
        .patch  = hook_RegisterTouchWindow,
        .link   = (void **) &next_RegisterTouchWindow
    },
    {
        .name   = "GetTouchInputInfo",
        .patch  = hook_GetTouchInputInfo,
        .link   = (void **) &next_GetTouchInputInfo
    },
    {
        .name   = "SetCursor",
        .patch  = hook_SetCursor,
        .link   = (void **) &next_SetCursor
    },
};

void touch_screen_hook_init(const struct touch_screen_config *cfg, HINSTANCE self)
{
    assert(cfg != NULL);

    if (!cfg->enable) {
        return;
    }

    if (touch_hook_initted) {
        return;
    }

    touch_hook_initted = true;

    defaultCursor = LoadCursorA(NULL, IDC_CROSS);

    memcpy(&touch_config, cfg, sizeof(*cfg));
    touch_hook_insert_hooks(NULL);
    dprintf("TOUCH: hook enabled.\n");
}

void touch_hook_insert_hooks(HMODULE target)
{
    hook_table_apply(
            target,
            "user32.dll",
            touch_hooks,
            _countof(touch_hooks));
}

static HCURSOR WINAPI hook_SetCursor(HCURSOR cursor) {
    if (cursor == 0 && touch_config.cursor)
        return next_SetCursor(defaultCursor);

    return next_SetCursor(cursor);
}

// remap mouse events to touch events

static LRESULT hook_wndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    if (hWnd == registered_hWnd && (Msg == WM_LBUTTONDOWN || Msg == WM_LBUTTONUP || (touch_held && Msg == WM_MOUSEMOVE))) {
        orig_wndProc(hWnd, WM_TOUCH, 1, 1);
    }

    return orig_wndProc(hWnd, Msg, wParam, lParam);
}

static ATOM WINAPI hook_RegisterClassExA(
    WNDCLASSEXA* wndClass
) {
    if (wndClass->lpfnWndProc) {
        orig_wndProc = wndClass->lpfnWndProc;
        wndClass->lpfnWndProc = (WNDPROC) hook_wndProc;
    }

    return next_RegisterClassExA(wndClass);
}

// Spicetools misc/wintouchemu.cpp

static int WINAPI hook_GetSystemMetrics(
    int nIndex
)
{
    if (nIndex == 94) return 0x01 | 0x02 | 0x40 | 0x80;

    int orig = next_GetSystemMetrics(nIndex);

    return orig;
}

static BOOL WINAPI hook_RegisterTouchWindow(
    HWND hwnd,
    ULONG ulFlags
)
{
    registered_hWnd = hwnd;
    return true;
}

// Converting mouse event to touch event
static BOOL WINAPI hook_GetTouchInputInfo(
    HANDLE hTouchInput,
    UINT cInputs,
    PTOUCHINPUT pInputs,
    int cbSize
)
{
    bool result = false;
    int sw, sh, cw, ch;
    RECT cRect;
    sw = GetSystemMetrics(SM_CXSCREEN);
    sh = GetSystemMetrics(SM_CYSCREEN);
    GetClientRect(registered_hWnd, &cRect);
    cw = cRect.right - cRect.left;
    ch = cRect.bottom - cRect.top;
    static bool mouse_state_old = false;

    for (UINT input = 0; input < cInputs; input++) {
        TOUCHINPUT *touch_input = &pInputs[input];

        touch_input->x = 0;
        touch_input->y = 0;
        touch_input->hSource = NULL;
        touch_input->dwID = 0;
        touch_input->dwFlags = 0;
        touch_input->dwMask = 0;
        touch_input->dwTime = 0;
        touch_input->dwExtraInfo = 0;
        touch_input->cxContact = 0;
        touch_input->cyContact = 0;

        bool mouse_state = (GetKeyState(VK_LBUTTON) & 0x100) != 0;

        if (mouse_state || mouse_state_old) {
            POINT cursorPos;
            
            GetCursorPos(&cursorPos);

            if (touch_config.remap) {
                ScreenToClient(registered_hWnd, &cursorPos);
                cursorPos.x = (long)(cursorPos.x * ((double)sw / cw));
                cursorPos.y = (long)(cursorPos.y * ((double)sh / ch));
            }

            result = true;
            touch_input->x = cursorPos.x * 100;
            touch_input->y = cursorPos.y * 100;
            touch_input->hSource = hTouchInput;
            touch_input->dwID = 0;
            touch_input->dwFlags = 0;
            if (mouse_state && !mouse_state_old) {
                touch_input->dwFlags |= TOUCHEVENTF_DOWN;
                touch_held = true;
            } else if (mouse_state && mouse_state_old) {
                touch_input->dwFlags |= TOUCHEVENTF_MOVE;
            } else if (!mouse_state && mouse_state_old) {
                touch_input->dwFlags |= TOUCHEVENTF_UP;
                touch_held = false;
            }
            touch_input->dwMask = 0;
            touch_input->dwTime = 0;
            touch_input->dwExtraInfo = 0;
            touch_input->cxContact = 0;
            touch_input->cyContact = 0;
        }
        mouse_state_old = mouse_state;
    }

    return result;
}
