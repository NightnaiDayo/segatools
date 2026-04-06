#include <windows.h>

#include <process.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "divaio/divaio.h"
#include "divaio/config.h"

#include "util/env.h"
#include "util/dprintf.h"
#include "util/str.h"

static unsigned int __stdcall diva_io_slider_thread_proc(void *ctx);
static unsigned int __stdcall diva_io_touch_thread_proc(void *ctx);

static HRESULT diva_io_touch_config_apply(
        const struct diva_io_config *cfg);

static const wchar_t app_title[] = L"Hatsune Miku Project DIVA Arcade Future Tone";

static bool diva_io_coin;
static uint16_t diva_io_coins;
static HANDLE diva_io_slider_thread;
static bool diva_io_slider_stop_flag;
static struct diva_io_config diva_io_cfg;
static bool diva_io_config_initted = false;
static bool diva_io_window_focus = true;

static HANDLE diva_io_touch_thread;
static int diva_io_m1 = VK_LBUTTON;
static int diva_io_touch_points = 1;
static bool diva_io_touch_stop_flag;

uint16_t diva_io_get_api_version(void)
{
    return 0x0101;
}

HRESULT diva_io_jvs_init(void)
{
    HRESULT hr;

    if(!diva_io_config_initted) {
        diva_io_config_load(&diva_io_cfg, get_config_path());
        diva_io_config_initted = true;
    }

    hr = diva_io_touch_config_apply(&diva_io_cfg);

    return hr;
}

void diva_io_jvs_poll(uint8_t *opbtn_out, uint8_t *gamebtn_out)
{
    uint8_t opbtn;
    uint8_t gamebtn;
    size_t i;

    opbtn = 0;

    HWND hwnd = GetForegroundWindow();
    wchar_t window_title[MAX_PATH];
    GetWindowTextW(hwnd, window_title, MAX_PATH);

    if (!wstr_eq(window_title, app_title)) {
        if (diva_io_window_focus) {
            diva_io_window_focus = false;
        }
    } else if (!diva_io_window_focus) {
        diva_io_window_focus = true;
    }

    if (GetAsyncKeyState(diva_io_cfg.vk_test) & 0x8000) {
        opbtn |= DIVA_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(diva_io_cfg.vk_service) & 0x8000) {
        opbtn |= DIVA_IO_OPBTN_SERVICE;
    }

    for (i = 0 ; i < _countof(diva_io_cfg.vk_buttons) ; i++) {
        if (GetAsyncKeyState(diva_io_cfg.vk_buttons[i]) & 0x8000) {
            gamebtn |= 1 << i;
        }
    }

    *opbtn_out = opbtn;
    *gamebtn_out = gamebtn;
}

void diva_io_jvs_read_coin_counter(uint16_t *out)
{
    if (out == NULL) {
        return;
    }

    if (GetAsyncKeyState(diva_io_cfg.vk_coin) & 0x8000) {
        if (!diva_io_coin) {
            diva_io_coin = true;
            diva_io_coins++;
        }
    } else {
        diva_io_coin = false;
    }

    *out = diva_io_coins;
}

HRESULT diva_io_slider_init(void)
{
    return S_OK;
}

void diva_io_slider_start(diva_io_slider_callback_t callback)
{
    if (diva_io_slider_thread != NULL) {
        return;
    }

    diva_io_slider_thread = (HANDLE) _beginthreadex(
            NULL,
            0,
            diva_io_slider_thread_proc,
            callback,
            0,
            NULL);
}

void diva_io_slider_stop(void)
{
    diva_io_slider_stop_flag = true;

    WaitForSingleObject(diva_io_slider_thread, INFINITE);
    CloseHandle(diva_io_slider_thread);
    diva_io_slider_thread = NULL;
    diva_io_slider_stop_flag = false;
}

void diva_io_slider_set_leds(const uint8_t *rgb)
{}

static unsigned int __stdcall diva_io_slider_thread_proc(void *ctx)
{
    diva_io_slider_callback_t callback;
    uint8_t pressure_val;
    uint8_t pressure[32];
    size_t i;

    callback = ctx;

    while (!diva_io_slider_stop_flag) {
        for (i = 0 ; i < 8 ; i++) {
            if (GetAsyncKeyState(diva_io_cfg.vk_slider[i]) & 0x8000) {
                pressure_val = 20;
            } else {
                pressure_val = 0;
            }

            memset(&pressure[4 * i], pressure_val, 4);
        }

        callback(pressure);
        Sleep(1);
    }

    return 0;
}

HRESULT diva_io_led_init(void)
{
    return S_OK;
}

void diva_io_led_set_leds(uint8_t board, const uint8_t *rgb)
{
#if 0
    dprintf("DIVA LED: LEFT PARTITION RED:    %02X\n", rgb[0]);
    dprintf("DIVA LED: LEFT PARTITION GREEN:  %02X\n", rgb[1]);
    dprintf("DIVA LED: LEFT PARTITION BLUE:   %02X\n", rgb[2]);
    dprintf("DIVA LED: RIGHT PARTITION RED:   %02X\n", rgb[3]);
    dprintf("DIVA LED: RIGHT PARTITION GREEN: %02X\n", rgb[4]);
    dprintf("DIVA LED: RIGHT PARTITION BLUE:  %02X\n", rgb[5]);
    dprintf("DIVA LED: BTN TRIANGLE:          %02X\n", rgb[6]);
    dprintf("DIVA LED: BTN CROSS:             %02X\n", rgb[7]);
    dprintf("DIVA LED: BTN SQUARE:            %02X\n", rgb[8]);
    dprintf("DIVA LED: BTN CIRCLE:            %02X\n", rgb[9]);
#endif

    return;
}

HRESULT diva_io_touch_init()
{
    return S_OK;
}

static HRESULT diva_io_touch_config_apply(
        const struct diva_io_config *cfg
    )
{
    dprintf("Diva IO: Touch: --- Begin Configuration ---\n");

    if (wstr_ieq(cfg->touch_mode, L"mouse")) {
        dprintf("Diva IO: Touch: Mouse emulation\n");

        /* Work around GetAsyncKeyState returning physical mouse button states
           instead of logical */
        diva_io_m1 = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;

    } else {
        dprintf("Diva IO: Touch: Invalid touch mode \"%S\". Use 'mouse'."
                "\n", cfg->touch_mode);

        return E_INVALIDARG;
    }

    dprintf("Diva IO: Touch: --- End Configuration ---\n");
    return S_OK;
}

void diva_io_touch_start(diva_io_touch_callback_t callback)
{
    if (diva_io_touch_thread != NULL) {
        return;
    }

    diva_io_touch_thread = (HANDLE) _beginthreadex(
            NULL,
            0,
            diva_io_touch_thread_proc,
            callback,
            0,
            NULL);
}

void diva_io_touch_stop(void)
{
    diva_io_touch_stop_flag = true;

    WaitForSingleObject(diva_io_touch_thread, INFINITE);
    CloseHandle(diva_io_touch_thread);
    diva_io_touch_thread = NULL;
    diva_io_touch_stop_flag = false;
}

static unsigned int __stdcall diva_io_touch_thread_proc(void *ctx)
{
    diva_io_touch_callback_t callback;
    HWND hwnd;
    POINT point;
    BOOL ok;

    uint8_t status = 0;
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t last_x = 0;
    uint16_t last_y = 0;
    uint8_t id = 0;
    bool touch = false;

    callback = ctx;

    while (!diva_io_touch_stop_flag) {

        if (!diva_io_window_focus) {
            status = 0;
            x = 0;
            y = 0;
            id = 0;
            touch = false;
            goto end;
        }

        if (wstr_ieq(diva_io_cfg.touch_mode, L"mouse"))
        {
            if (GetAsyncKeyState(diva_io_m1) & 0x8000)
            {
                /* Get cursor location and map to window size */
                ok = GetCursorPos(&point);

                if (!ok) {
                    status = 0;
                    x = 0;
                    y = 0;
                    id = 0;
                    touch = false;
                    goto end;
                }

                hwnd = GetForegroundWindow();

                ok = ScreenToClient(hwnd, &point);

                if (!ok) {
                    status = 0;
                    x = 0;
                    y = 0;
                    id = 0;
                    touch = false;
                    goto end;
                }

                /* Set status */
                if (!touch) {
                    status = DIVA_IO_TOUCH_DOWN;
                    touch = true;
                } else {
                    status = DIVA_IO_TOUCH_STREAM;
                }

                /* Set coordinates */
                if (point.x < 0) point.x = 0;
                if (point.y < 0) point.y = 0;
                x = (uint16_t)point.x;
                y = (uint16_t)point.y;
                last_x = x;
                last_y = y;
            }
            else
            {
                if (touch) {
                    status = DIVA_IO_TOUCH_LIFTOFF;
                    x = last_x;
                    y = last_y;
                    touch = false;
                } else {
                    /* No touch event */
                    status = 0;
                    x = 0;
                    y = 0;
                }
            }

            /* Mouse always acts as single-touch */
            id = 1;
        }

end:
        callback(status, x, y, id);
        Sleep(1);
    }

    return 0;
}
