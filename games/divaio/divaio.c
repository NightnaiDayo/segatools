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

#define DIVA_SLIDER_AUTO_FRAME_DELAY 5

static unsigned int __stdcall diva_io_slider_thread_proc(void* ctx);
static unsigned int __stdcall diva_io_touch_thread_proc(void* ctx);

static LARGE_INTEGER performanceFrequency;

static struct diva_io_config diva_io_cfg;
static const struct diva_io_backend* diva_io_backend;
static bool diva_io_coin;
static uint16_t diva_io_coins;
static struct diva_button_states diva_button_states = {0};

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


    QueryPerformanceFrequency(&performanceFrequency);

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

        return diva_kb_init(&diva_io_cfg.kb, &diva_io_backend);
    } else if (wstr_ieq(diva_io_cfg.input_mode, L"dinput")) {
        dprintf("Diva IO: Using DirectInput\n");

        return diva_di_init(&diva_io_cfg.di, inst, &diva_io_backend);
    } else if (wstr_ieq(diva_io_cfg.input_mode, L"xinput")) {
        dprintf("Diva IO: Using XInput\n");

        return diva_xi_init(&diva_io_cfg.xi, &diva_io_backend);
    } else {
        dprintf("Diva IO: Invalid input mode \"%S\". Use 'keyboard', 'dinput' or 'xinput'.\n", diva_io_cfg.touch_mode);

        return E_INVALIDARG;
    }

}

inline static void diva_io_reset_hold_timeouts(struct diva_button_state* state) {
    ULONGLONG ticks = GetTickCount64();
    state->doubleTapTimeoutBegin = ticks + diva_io_cfg.hold_transfer_time_min;
    state->doubleTapTimeoutEnd = ticks + diva_io_cfg.hold_transfer_time_max;
}

static void diva_io_set_game_button(uint8_t* gamebtn, int input_bit, struct diva_button_state* state) {

    if (state->dropFrames > 0) {
        state->dropFrames--;
        return;
    }

    // anything pressed
    if (state->primary || state->secondary) {

        // just pressed anything
        if ((state->primary || state->secondary) && !(state->prevPrimary || state->prevSecondary)) {
            diva_io_reset_hold_timeouts(state);
        }

        // just pressed both in the same frame
        // OR if we switched buttons exactly in the same frame, this is also a double-tap!
        if (
            (state->primary && state->secondary && !(state->prevPrimary && state->prevSecondary)) ||
            (!state->primary && state->prevPrimary && state->secondary && !state->prevSecondary) ||
            (state->primary && !state->prevPrimary && !state->secondary && state->prevSecondary)
            ) {

            // if we are outside the takeover range, add dropped frame(s) to simulate re-press
            ULONGLONG ticks = GetTickCount64();
            if (ticks < state->doubleTapTimeoutBegin || ticks > state->doubleTapTimeoutEnd) {
                state->dropFrames = diva_io_cfg.dropped_input_frames;

                // also reset double-tap timer for quick alternations
                diva_io_reset_hold_timeouts(state);
            }

            // otherwise it's a hold-takeover, do nothing special
        }

        if (state->dropFrames == 0) {
            *gamebtn |= input_bit;
        }
    }

    state->prevPrimary = state->primary;
    state->prevSecondary = state->secondary;
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

    if (GetAsyncKeyState(diva_io_cfg.vk_test) & 0x8000) {
        opbtn |= DIVA_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(diva_io_cfg.vk_service) & 0x8000) {
        opbtn |= DIVA_IO_OPBTN_SERVICE;
    }

    if (diva_io_backend->get_gamebtns != NULL) {
        diva_io_backend->get_gamebtns(&diva_button_states);
    }

    if (diva_button_states.start) {
        gamebtn |= DIVA_IO_GAMEBTN_START;
    }

    diva_io_set_game_button(&gamebtn, DIVA_IO_GAMEBTN_CIRCLE, &diva_button_states.circle);
    diva_io_set_game_button(&gamebtn, DIVA_IO_GAMEBTN_CROSS, &diva_button_states.cross);
    diva_io_set_game_button(&gamebtn, DIVA_IO_GAMEBTN_TRIANGLE, &diva_button_states.triangle);
    diva_io_set_game_button(&gamebtn, DIVA_IO_GAMEBTN_SQUARE, &diva_button_states.square);

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

    if (diva_io_backend == NULL) {
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
    LARGE_INTEGER last_auto_time, current_auto_time;

    const diva_io_slider_callback_t callback = ctx;

    QueryPerformanceCounter(&last_auto_time);

    while (!diva_io_slider_stop_flag) {

        // Move the auto-slider every 30 ms to simulate constant movement. It's only actually
        // sent to the game if the IO backend returns true for either left or right hand.
        QueryPerformanceCounter(&current_auto_time);
        if ((double)(current_auto_time.QuadPart - last_auto_time.QuadPart) / ((double)performanceFrequency.QuadPart / 1000) > 30) {
            // Wrap around the 32 cells whenever we hit the edges.
            // Ensure we have a few frames of no-touch when wrapping around, otherwise
            // the game misdetects this as a very far swipe into the other direction,
            // causing the menu to scroll back and forth.
            // Unsure if there's a better solution.
            if (++slider_auto_right_pos >= DIVA_SLIDER_CELL_COUNT + DIVA_SLIDER_AUTO_FRAME_DELAY) {
                slider_auto_right_pos = DIVA_SLIDER_CELL_COUNT / 2;
            }
            if (--slider_auto_left_pos < -DIVA_SLIDER_AUTO_FRAME_DELAY) {
                slider_auto_left_pos = DIVA_SLIDER_CELL_COUNT / 2 - 1;
            }

            last_auto_time = current_auto_time;
        }

        slider_status = 0;

        if (diva_io_backend->get_slider != NULL) {
            diva_io_backend->get_slider(&slider_status);
        }

        if (diva_io_backend->get_auto_status != NULL) {
            bool l;
            bool r;

            diva_io_backend->get_auto_status(&l, &r);

            if (l && slider_auto_left_pos > -1 && slider_auto_left_pos < DIVA_SLIDER_CELL_COUNT) {
                slider_status |= 1 << slider_auto_left_pos;
            }
            if (r && slider_auto_right_pos > -1 && slider_auto_right_pos < DIVA_SLIDER_CELL_COUNT) {
                slider_status |= 1 << slider_auto_right_pos;
            }

            // Reset auto-sliders to center if they aren't moved
            if (!l && !r) {
                slider_auto_left_pos = DIVA_SLIDER_CELL_COUNT / 2 - 1;
                slider_auto_right_pos = DIVA_SLIDER_CELL_COUNT / 2;
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
