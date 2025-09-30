#include "io4.h"

#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "board/io4.h"

#include "sekitohook/sekito-dll.h"

#include "util/dprintf.h"

static HRESULT sekito_io4_poll(void *ctx, struct io4_state *state);
static uint16_t coins;

static const struct io4_ops sekito_io4_ops = {
    .poll = sekito_io4_poll,
};

static bool io_is_terminal;

HRESULT sekito_io4_hook_init(const struct io4_config *cfg, bool is_terminal)
{
    HRESULT hr;

    assert(sekito_dll.init != NULL);

    hr = io4_hook_init(cfg, &sekito_io4_ops, NULL);

    io_is_terminal = is_terminal;

    if (FAILED(hr)) {
        return hr;
    }

    return sekito_dll.init();
}

static HRESULT sekito_io4_poll(void *ctx, struct io4_state *state)
{
    uint8_t opbtn;
    uint16_t x, y;
    uint32_t gamebtn;
    HRESULT hr;

    assert(sekito_dll.poll != NULL);
    assert(sekito_dll.get_opbtns != NULL);
    assert(sekito_dll.get_gamebtns != NULL);
    assert(sekito_dll.get_trackball_position != NULL);

    memset(state, 0, sizeof(*state));

    hr = sekito_dll.poll();

    if (FAILED(hr)) {
        return hr;
    }

    opbtn = 0;
    gamebtn = 0;
    x = 0;
    y = 0;

    sekito_dll.get_opbtns(&opbtn);
    sekito_dll.get_gamebtns(&gamebtn);
    sekito_dll.get_trackball_position(&x, &y);

    if (opbtn & SEKITO_IO_OPBTN_TEST) {
        state->buttons[0] |= IO4_BUTTON_TEST;
    }

    if (opbtn & SEKITO_IO_OPBTN_SERVICE) {
        state->buttons[0] |= IO4_BUTTON_SERVICE;
    }

    if (opbtn & SEKITO_IO_OPBTN_SW1) {
        state->buttons[0] |= 1 << 10;
    }

    if (opbtn & SEKITO_IO_OPBTN_SW2) {
        state->buttons[0] |= 1 << 11;
    }

    if (opbtn & SEKITO_IO_OPBTN_COIN) {
        coins++;
    }
    state->chutes[0] = coins << 8;

    if (!io_is_terminal) {
        if (gamebtn & SEKITO_IO_GAMEBTN_HOUGU) {
            state->buttons[1] |= 1 << 6;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_MENU) {
            state->buttons[1] |= 1 << 4;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_START) {
            state->buttons[0] |= 1 << 15;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_STRATAGEM) {
            state->buttons[1] |= 1 << 7;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_STRATAGEM_LOCK) {
            state->buttons[1] |= 1 << 5;
        }

    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_0) {
        state->buttons[0] |= SEKITO_NUMPAD_C2;
        state->buttons[0] |= SEKITO_NUMPAD_R4;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_1) {
        state->buttons[0] |= SEKITO_NUMPAD_C1;
        state->buttons[0] |= SEKITO_NUMPAD_R1;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_2) {
        state->buttons[0] |= SEKITO_NUMPAD_C2;
        state->buttons[0] |= SEKITO_NUMPAD_R1;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_3) {
        state->buttons[0] |= SEKITO_NUMPAD_C3;
        state->buttons[0] |= SEKITO_NUMPAD_R1;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_4) {
        state->buttons[0] |= SEKITO_NUMPAD_C1;
        state->buttons[0] |= SEKITO_NUMPAD_R2;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_5) {
        state->buttons[0] |= SEKITO_NUMPAD_C2;
        state->buttons[0] |= SEKITO_NUMPAD_R2;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_6) {
        state->buttons[0] |= SEKITO_NUMPAD_C3;
        state->buttons[0] |= SEKITO_NUMPAD_R2;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_7) {
        state->buttons[0] |= SEKITO_NUMPAD_C1;
        state->buttons[0] |= SEKITO_NUMPAD_R3;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_8) {
        state->buttons[0] |= SEKITO_NUMPAD_C2;
        state->buttons[0] |= SEKITO_NUMPAD_R3;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_9) {
        state->buttons[0] |= SEKITO_NUMPAD_C3;
        state->buttons[0] |= SEKITO_NUMPAD_R3;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_CLEAR) {
        state->buttons[0] |= SEKITO_NUMPAD_C1;
        state->buttons[0] |= SEKITO_NUMPAD_R4;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_ENTER) {
        state->buttons[0] |= SEKITO_NUMPAD_C3;
        state->buttons[0] |= SEKITO_NUMPAD_R4;
    }

    if (io_is_terminal) {
        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_CANCEL) {
            state->buttons[1] |= 1 << 0;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_DECIDE) {
            state->buttons[1] |= 1 << 1;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_LEFT) {
            state->buttons[0] |= 1 << 3;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_UP) {
            state->buttons[0] |= 1 << 5;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_RIGHT) {
            state->buttons[0] |= 1 << 2;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_DOWN) {
            state->buttons[0] |= 1 << 4;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_LEFT_2) {
            state->buttons[1] |= 1 << 3;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_RIGHT_2) {
            state->buttons[1] |= 1 << 2;
        }
    }

    state->spinners[2] = x;
    state->spinners[3] = y;

    return S_OK;
}
