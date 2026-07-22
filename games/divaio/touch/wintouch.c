#include "backend.h"
#include "util/dprintf.h"

#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

#define DEBUG 0

static HRESULT diva_wintouch_init();
static void diva_wintouch_update(diva_io_touch_callback_t callback);
static void diva_handle_touch(UINT cInputs, PTOUCHINPUT pInputs);

static const struct diva_touch_backend diva_wintouch_backend = {
    .init   = diva_wintouch_init,
    .update = diva_wintouch_update
};

static HWND window_handle = NULL;
static const wchar_t* window_title = L"Hatsune Miku Project DIVA Arcade Future Tone";
static WNDPROC original_wndproc = NULL;

static uint8_t status = 0;
static uint16_t x = 0;
static uint16_t y = 0;
static bool touch = false;

struct handle_data {
    unsigned long process_id;
    HWND window_handle;
};

BOOL is_main_window(HWND handle) {
    return GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle);
}

BOOL CALLBACK enum_windows_callback(HWND handle, LPARAM lParam) {
    struct handle_data* data = (struct handle_data*)lParam;
    unsigned long process_id = 0;
    GetWindowThreadProcessId(handle, &process_id);
    if (data->process_id != process_id || !is_main_window(handle))
        return TRUE;
    data->window_handle = handle;
    return FALSE;
}

HWND find_main_window(unsigned long process_id) {
    struct handle_data data = {0};
    data.process_id = process_id;
    data.window_handle = 0;
    EnumWindows(enum_windows_callback, (LPARAM)&data);
    return data.window_handle;
}

static HWND get_window() {
    if (window_handle != NULL) {
        return window_handle;
    }

    HWND hwnd = find_main_window(GetCurrentProcessId());
    if (hwnd == NULL) {
        return NULL;
    }

    dprintf("Diva IO: Touch: Program window detected\n");
    window_handle = hwnd;
    return window_handle;
}

HRESULT diva_touch_wintouch_init(const struct diva_touch_backend** backend) {
    if (!(GetSystemMetrics(SM_DIGITIZER) & NID_READY)) {
        dprintf("Diva IO: Touch: No touchscreen connected!\n");
        return E_FAIL;
    }

    *backend = &diva_wintouch_backend;

    return S_OK;
}

LRESULT CALLBACK diva_touch_wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TOUCH) {
        const UINT cInputs = LOWORD(wParam);
        const PTOUCHINPUT pInputs = (PTOUCHINPUT)malloc(sizeof(TOUCHINPUT) * cInputs);

        if (pInputs == NULL) {
            dprintf("Diva IO: Touch: Allocation error\n");
            return 0;
        }

        if (GetTouchInputInfo((HTOUCHINPUT)lParam, cInputs, pInputs, sizeof(TOUCHINPUT))) {
            diva_handle_touch(cInputs, pInputs);
        }

        free(pInputs);
        CloseTouchInputHandle((HTOUCHINPUT)lParam);

        return 0;
    } else if (original_wndproc != NULL) {
        return CallWindowProcW(original_wndproc, hWnd, msg, wParam, lParam);
    } else {
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

HRESULT diva_wintouch_init() {
    window_handle = get_window();
    if (window_handle == NULL) {
        dprintf("Diva IO: Touch: Failed to find main window\n");
        return E_FAIL;
    }

    if (!RegisterTouchWindow(window_handle, 0)) {
        dprintf("Diva IO: Touch: Failed to register touch window: %lx\n", GetLastError());
        return E_FAIL;
    }

    original_wndproc = (WNDPROC)GetWindowLongPtrW(window_handle, GWLP_WNDPROC);
    if (original_wndproc == NULL) {
        dprintf("Diva IO: Touch: Failed to retrieve original window procedure: %lx\n", GetLastError());
        return E_FAIL;
    }

    if (!SetWindowLongPtrW(window_handle, GWLP_WNDPROC, (LONG_PTR)&diva_touch_wndproc)) {
        dprintf("Diva IO: Touch: Failed to set new window procedure: %lx\n", GetLastError());
        return E_FAIL;
    }

    dprintf("Diva IO: Touch: WinTouch initialized\n");
    return S_OK;
}

void diva_wintouch_update(diva_io_touch_callback_t callback) {
    if (status == DIVA_IO_TOUCH_LIFTOFF) {
        status = 0;
        touch = false;
    } else if (!touch) {
        status = 0;
        x = 0;
        y = 0;
    }

    callback(status, x, y, 1);
}

static void diva_handle_touch(const UINT cInputs, const PTOUCHINPUT pInputs) {
    if (cInputs == 0) {
        return;
    }

    // The Elo touchscreen is single touch, so we ignore everything except the first touch.
    const TOUCHINPUT ti = pInputs[0];

    if (ti.dwFlags & (TOUCHEVENTF_DOWN | TOUCHEVENTF_MOVE)) {
        touch = true;
        status = ti.dwFlags & TOUCHEVENTF_DOWN ? DIVA_IO_TOUCH_DOWN : DIVA_IO_TOUCH_STREAM;
    } else if (ti.dwFlags & TOUCHEVENTF_UP) {
        status = DIVA_IO_TOUCH_LIFTOFF;
    }

    x = max(0, ti.x / 100);
    y = max(0, ti.y / 100);

}
