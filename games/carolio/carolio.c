#include <windows.h>

#include <process.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "carolio/carolio.h"
#include "carolio/config.h"
#include "util/dprintf.h"
#include "util/env.h"

static unsigned int __stdcall carol_io_touch_thread_proc(void *ctx);

static bool carol_io_coin;
static uint16_t carol_io_coins;
static struct carol_io_config carol_io_cfg;
static bool carol_io_touch_stop_flag;
static HANDLE carol_io_touch_thread;
static bool carol_io_window_focus = false;

uint16_t carol_io_get_api_version(void)
{
    return 0x0100;
}

HRESULT carol_io_jvs_init(void)
{
    carol_io_config_load(&carol_io_cfg, get_config_path());

    return S_OK;
}

void carol_io_jvs_poll(uint8_t *opbtn_out, uint8_t *gamebtn_out)
{
    uint8_t opbtn;
    uint8_t gamebtn;
    size_t i;

    opbtn = 0;

    if (GetAsyncKeyState(carol_io_cfg.vk_test) & 0x8000) {
        opbtn |= 1;
    }

    if (GetAsyncKeyState(carol_io_cfg.vk_service) & 0x8000) {
        opbtn |= 2;
    }

    for (i = 0 ; i < _countof(carol_io_cfg.vk_buttons) ; i++) {
        if (GetAsyncKeyState(carol_io_cfg.vk_buttons[i]) & 0x8000) {
            gamebtn |= 1 << i;
        }
    }

    *opbtn_out = opbtn;
    *gamebtn_out = gamebtn;
}

void carol_io_jvs_read_coin_counter(uint16_t *out)
{
    if (out == NULL) {
        return;
    }

    if (GetAsyncKeyState(carol_io_cfg.vk_coin) & 0x8000) {
        if (!carol_io_coin) {
            carol_io_coin = true;
            carol_io_coins++;
        }
    } else {
        carol_io_coin = false;
    }

    *out = carol_io_coins;
}

HRESULT carol_io_touch_init()
{
    return S_OK;
}

HRESULT carol_io_ledbd_init()
{
    return S_OK;
}

HRESULT carol_io_controlbd_init()
{
    return S_OK;
}

void carol_io_touch_start(carol_io_touch_callback_t callback)
{
    if (carol_io_touch_thread != NULL) {
        return;
    }

    carol_io_touch_stop_flag = false;

    carol_io_touch_thread = (HANDLE) _beginthreadex(
        NULL,
        0,
        carol_io_touch_thread_proc,
        callback,
        0,
        NULL
    );
}

void carol_io_touch_stop()
{
    carol_io_touch_stop_flag = true;
}

void check_fg_wind(void)
{
    HWND hwnd = GetForegroundWindow();
    wchar_t window_class[MAX_PATH];

    /* Unlike every other game, we can't use GetWindowText here. Why?

        From MSDN:

        "If the window does not have a caption, the return value is a null
        string. This behavior is by design. It allows applications to call
        GetWindowText without becoming unresponsive if the process that owns
        the target window is not responding. However, if the target window
        is not responding and it belongs to the calling application,
        GetWindowText will cause the calling application to become
        unresponsive."

        Great, thanks Microsoft, very cool. Luckily Carol sets its class
        name to the window title too so we can use that. */

    GetClassNameW(hwnd, window_class, MAX_PATH);

    if (wcscmp(window_class, L"WONDER Master")) {
        if (carol_io_window_focus) {
            dprintf("Carol IO: Window focus lost\n");
            carol_io_window_focus = false;
        }
    } else if (!carol_io_window_focus) {
        dprintf("Carol IO: Window focus regained\n");
        carol_io_window_focus = true;
    }
}

static unsigned int __stdcall carol_io_touch_thread_proc(void *ctx)
{
    carol_io_touch_callback_t callback;
    bool mouse_is_down = false;
    uint16_t mX = 0;
    uint16_t mY = 0;
    POINT lpPoint;
    HWND hwnd;

    callback = ctx;

    while (!carol_io_touch_stop_flag) {
        check_fg_wind();
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
            mouse_is_down = true;
            if (GetCursorPos(&lpPoint)) {
                hwnd = GetForegroundWindow();
                if (ScreenToClient(hwnd, &lpPoint)) {
                    if (lpPoint.x < 0) lpPoint.x = 0;
                    if (lpPoint.y < 0) lpPoint.y = 0;
                    mX = (uint16_t)lpPoint.x;
                    mY = (uint16_t)lpPoint.y;
                }
            }
        } else {
            mouse_is_down = false;
        }

        callback(mouse_is_down, mX, mY);
        Sleep(1);
    }

    return 0;
}
