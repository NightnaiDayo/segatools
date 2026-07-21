#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "board/io4.h"

#include "hkbhook/hkb-dll.h"

#include "util/dprintf.h"

static HRESULT hkb_io4_poll(void* ctx, struct io4_state* state);
static uint16_t coins;
static bool io_is_terminal;

static const struct io4_ops hkb_io4_ops = {.poll = hkb_io4_poll,};

HRESULT hkb_io4_hook_init(const struct io4_config* cfg, bool is_terminal) {
    HRESULT hr;

    assert(hkb_dll.init != NULL);

    io_is_terminal = is_terminal;

    hr = io4_hook_init(cfg, &hkb_io4_ops, NULL, L"hkb", false);

    if (FAILED(hr)) {
        return hr;
    }

    return hkb_dll.init();
}

static HRESULT hkb_io4_poll(void* ctx, struct io4_state* state) {
    uint8_t opbtn;
    uint8_t gamebtn;
    HRESULT hr;

    assert(hkb_dll.poll != NULL);
    assert(hkb_dll.get_opbtns != NULL);
    assert(hkb_dll.get_gamebtns != NULL);

    memset(state, 0, sizeof(*state));

    hr = hkb_dll.poll();

    if (FAILED(hr)) {
        return hr;
    }

    opbtn = 0;
    gamebtn = 0;

    hkb_dll.get_opbtns(&opbtn);
    hkb_dll.get_gamebtns(&gamebtn);

    if (opbtn & HKB_IO_OPBTN_TEST) {
        state->buttons[0] |= IO4_BUTTON_TEST;
    }

    if (opbtn & HKB_IO_OPBTN_SERVICE) {
        state->buttons[0] |= IO4_BUTTON_SERVICE;
    }

    if (opbtn & HKB_IO_OPBTN_COIN) {
        coins++;
    }
    state->chutes[0] = coins << 8;

    if (io_is_terminal) {
        if (gamebtn & HKB_IO_GAMEBTN_TERMINAL_CANCEL) {
            state->buttons[1] |= 1 << 12;
        }

        if (gamebtn & HKB_IO_GAMEBTN_TERMINAL_DECIDE) {
            state->buttons[1] |= 1 << 13;
        }

        if (gamebtn & HKB_IO_GAMEBTN_TERMINAL_LEFT) {
            state->buttons[1] |= 1 << 0;
        }

        if (gamebtn & HKB_IO_GAMEBTN_TERMINAL_UP) {
            state->buttons[1] |= 1 << 15;
        }

        if (gamebtn & HKB_IO_GAMEBTN_TERMINAL_RIGHT) {
            state->buttons[1] |= 1 << 1;
        }

        if (gamebtn & HKB_IO_GAMEBTN_TERMINAL_DOWN) {
            state->buttons[1] |= 1 << 14;
        }

        if (gamebtn & HKB_IO_GAMEBTN_TERMINAL_LEFT_2) {
            state->buttons[1] |= 1 << 3;
        }

        if (gamebtn & HKB_IO_GAMEBTN_TERMINAL_RIGHT_2) {
            state->buttons[1] |= 1 << 2;
        }
    }

    return S_OK;
}