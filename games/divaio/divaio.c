#include <windows.h>

#include <assert.h>
#include <process.h>
#include <stdint.h>
#include <stdlib.h>

#include "divaio/config.h"
#include "divaio/divaio.h"
#include "divaio/input/backend.h"
#include "divaio/touch/backend.h"
#include "divaio/touch/mouse.h"
#include "divaio/touch/wintouch.h"
#include "input/di.h"
#include "input/kb.h"
#include "input/xi.h"

#include "util/env.h"
#include "util/dprintf.h"
#include "util/fg-detect.h"
#include "util/str.h"

static unsigned int __stdcall diva_io_slider_thread_proc(void* ctx);
static unsigned int __stdcall diva_io_touch_thread_proc(void* ctx);

static struct diva_io_config diva_io_cfg;
static const struct diva_io_backend* diva_io_backend;
static bool diva_io_coin;
static uint16_t diva_io_coins;

static HANDLE diva_io_slider_thread;
static bool diva_io_slider_stop_flag;
static int8_t slider_auto_left_pos = 0;
static int8_t slider_auto_right_pos = DIVA_SLIDER_CELL_COUNT;

static const struct diva_touch_backend* diva_touch_backend;
static HANDLE diva_io_touch_thread;
static bool diva_io_touch_stop_flag;

uint16_t diva_io_get_api_version(void) {
    return 0x0101;
}

HRESULT diva_io_jvs_init(void) {
    assert(diva_io_backend == NULL);

    HINSTANCE inst = GetModuleHandleW(NULL);

    if (inst == NULL) {
        HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
        dprintf("GetModuleHandleW failed: %lx\n", hr);

        return hr;
    }

    diva_io_config_load(&diva_io_cfg, get_config_path());

    dprintf("Diva IO: Touch: --- Begin Configuration ---\n");

    if (wstr_ieq(diva_io_cfg.touch_mode, L"mouse")) {
        dprintf("Diva IO: Touch: Mouse emulation\n");

        diva_touch_mouse_init(&diva_touch_backend);
    } else if (wstr_ieq(diva_io_cfg.touch_mode, L"wintouch")) {
        dprintf("Diva IO: Touch: WinTouch conversion\n");

        diva_touch_wintouch_init(&diva_touch_backend);
    } else {
        dprintf("Diva IO: Touch: Invalid touch mode \"%S\". Use 'mouse' or 'keyboard'.\n", diva_io_cfg.touch_mode);

        return E_INVALIDARG;
    }

    dprintf("Diva IO: Touch: --- End Configuration ---\n");

    if (wstr_ieq(diva_io_cfg.input_mode, L"keyboard")) {
        dprintf("Diva IO: Using keyboard\n");

        diva_kb_init(&diva_io_cfg.kb, &diva_io_backend);
    } else if (wstr_ieq(diva_io_cfg.input_mode, L"dinput")) {
        dprintf("Diva IO: Using DirectInput\n");

        diva_di_init(&diva_io_cfg.di, inst, &diva_io_backend);
    } else if (wstr_ieq(diva_io_cfg.input_mode, L"xinput")) {
        dprintf("Diva IO: Using XInput\n");

        diva_xi_init(&diva_io_cfg.xi, &diva_io_backend);
    } else {
        dprintf("Diva IO: Invalid input mode \"%S\". Use 'keyboard', 'dinput' or 'xinput'.\n", diva_io_cfg.touch_mode);

        return E_INVALIDARG;
    }

    return S_OK;
}

void diva_io_jvs_poll(uint8_t* opbtn_out, uint8_t* gamebtn_out) {
    assert(opbtn_out != NULL);
    assert(gamebtn_out != NULL);
    assert(diva_io_backend != NULL);

    uint8_t opbtn = 0;
    uint8_t gamebtn = 0;

    if (diva_io_backend->get_opbtns != NULL) {
        diva_io_backend->get_opbtns(&opbtn);
    }

    if (diva_io_backend->get_gamebtns != NULL) {
        diva_io_backend->get_gamebtns(&gamebtn);
    }

    if (GetAsyncKeyState(diva_io_cfg.vk_test) & 0x8000) {
        opbtn |= DIVA_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(diva_io_cfg.vk_service) & 0x8000) {
        opbtn |= DIVA_IO_OPBTN_SERVICE;
    }

    *opbtn_out = opbtn;
    *gamebtn_out = gamebtn;
}

void diva_io_jvs_read_coin_counter(uint16_t* out) {
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

HRESULT diva_io_slider_init(void) {
    return S_OK;
}

void diva_io_slider_start(diva_io_slider_callback_t callback) {
    if (diva_io_slider_thread != NULL) {
        return;
    }

    diva_io_slider_thread = (HANDLE)_beginthreadex(
        NULL,
        0,
        diva_io_slider_thread_proc,
        callback,
        0,
        NULL);
}

void diva_io_slider_stop(void) {
    diva_io_slider_stop_flag = true;

    WaitForSingleObject(diva_io_slider_thread, INFINITE);
    CloseHandle(diva_io_slider_thread);
    diva_io_slider_thread = NULL;
    diva_io_slider_stop_flag = false;
}

void diva_io_slider_set_leds(const uint8_t* rgb) {
}

static unsigned int __stdcall diva_io_slider_thread_proc(void* ctx) {
    uint8_t pressure[DIVA_SLIDER_CELL_COUNT];
    uint32_t slider_status = 0;
    LARGE_INTEGER freq, last_auto_time, current_auto_time;

    const diva_io_slider_callback_t callback = ctx;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last_auto_time);

    while (!diva_io_slider_stop_flag) {

        // Move the auto-slider every 30 ms to simulate constant movement. It's only actually
        // sent to the game if the IO backend returns true for either left or right hand.
        QueryPerformanceCounter(&current_auto_time);
        if ((double)(current_auto_time.QuadPart - last_auto_time.QuadPart) / ((double)freq.QuadPart / 1000) > 30) {
            // Wrap around the 32 cells whenever we hit the edges. Unsure if there's a better solution.
            if (++slider_auto_right_pos >= DIVA_SLIDER_CELL_COUNT) {
                slider_auto_right_pos = 0;
            }
            if (--slider_auto_left_pos < 0) {
                slider_auto_left_pos = DIVA_SLIDER_CELL_COUNT - 1;
            }

            last_auto_time = current_auto_time;
        }

        slider_status = 0;

        if (diva_io_backend->get_slider != NULL) {
            diva_io_backend->get_slider(&slider_status);
        }

        if (diva_io_backend->get_auto_status != NULL) {
            bool l, r;

            diva_io_backend->get_auto_status(&l, &r);

            if (l) {
                slider_status |= 1 << slider_auto_left_pos;
            }
            if (r) {
                slider_status |= 1 << slider_auto_right_pos;
            }
        }

        for (int i = 0; i < DIVA_SLIDER_CELL_COUNT; i++) {
            pressure[i] = ((slider_status >> i) & 1) != 0 ? 20 : 0;
        }

        callback(pressure);
        Sleep(1);
    }

    return 0;
}

HRESULT diva_io_led_init(void) {
    return S_OK;
}

void diva_io_led_set_leds(uint8_t board, const uint8_t* rgb) {
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
}

HRESULT diva_io_touch_init() {
    if (diva_touch_backend->init != NULL) {
        return diva_touch_backend->init();
    }

    return S_OK;
}

void diva_io_touch_start(diva_io_touch_callback_t callback) {
    if (diva_io_touch_thread != NULL) {
        return;
    }

    diva_io_touch_thread = (HANDLE)_beginthreadex(
        NULL,
        0,
        diva_io_touch_thread_proc,
        callback,
        0,
        NULL);
}

void diva_io_touch_stop(void) {
    diva_io_touch_stop_flag = true;

    WaitForSingleObject(diva_io_touch_thread, INFINITE);
    CloseHandle(diva_io_touch_thread);
    diva_io_touch_thread = NULL;
    diva_io_touch_stop_flag = false;
}

static unsigned int __stdcall diva_io_touch_thread_proc(void* ctx) {
    const diva_io_touch_callback_t callback = ctx;

    while (!diva_io_touch_stop_flag) {

        if (diva_touch_backend->update != NULL) {
            diva_touch_backend->update(callback);
        }

        Sleep(1);
    }

    return 0;
}
