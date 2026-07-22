#include "backend.h"
#include "util/dprintf.h"

#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

static void diva_wintouch_update(diva_io_touch_callback_t callback);
static void diva_handle_touch(UINT cInputs, PTOUCHINPUT pInputs);

static const struct diva_touch_backend diva_wintouch_backend = {
    .update = diva_wintouch_update
};

static HWND window_handle = NULL;
static const wchar_t* window_title = L"Hatsune Miku Project DIVA Arcade Future Tone";
static WNDPROC original_wndproc = NULL;

static uint8_t status = 0;
static uint16_t x = 0;
static uint16_t y = 0;
static bool touch = false;

static HWND get_window() {
    if (window_handle != NULL) {
        return window_handle;
    }

    wchar_t scanned_title[256];

    // try detecting the window
    HWND hwnd = GetForegroundWindow();
    if (GetWindowTextW(hwnd, scanned_title, sizeof(scanned_title)) == 0) {
        return NULL;
    }
    if (wcscmp(scanned_title, window_title) != 0) {
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

void diva_wintouch_update(diva_io_touch_callback_t callback) {
    if (window_handle == NULL) {
        window_handle = get_window();
        if (window_handle == NULL) {
            return;
        }

        if (!RegisterTouchWindow(window_handle, 0)) {
            dprintf("Diva IO: Touch: Failed to register touch window: %lx\n", GetLastError());
            return;
        }

        original_wndproc = (WNDPROC)GetWindowLongPtrW(window_handle, GWLP_WNDPROC);
        if (original_wndproc == NULL) {
            dprintf("Diva IO: Touch: Failed to retrieve original window procedure: %lx\n", GetLastError());
            return;
        }

        if (!SetWindowLongPtrW(window_handle, GWLP_WNDPROC, (LONG_PTR)&diva_touch_wndproc)) {
            dprintf("Diva IO: Touch: Failed to set new window procedure: %lx\n", GetLastError());
            return;
        }

        dprintf("Diva IO: Touch: WinTouch initialized\n");
    }

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

    x = max(0, ti.x);
    y = max(0, ti.y);
}
