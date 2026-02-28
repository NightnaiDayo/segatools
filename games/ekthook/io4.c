#include "io4.h"

#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "board/io4.h"

#include "ekthook/ekt-dll.h"

#include "util/dprintf.h"

static HRESULT ekt_io4_poll(void *ctx, struct io4_state *state);
static uint16_t coins;

static const struct io4_ops ekt_io4_ops = {
    .poll = ekt_io4_poll,
};

static bool io_is_terminal;

HRESULT ekt_io4_hook_init(const struct io4_config *cfg, bool is_terminal)
{
    HRESULT hr;

    assert(ekt_dll.init != NULL);

    hr = io4_hook_init(cfg, &ekt_io4_ops, NULL, L"ekt", false);

    io_is_terminal = is_terminal;

    if (FAILED(hr)) {
        return hr;
    }

    return ekt_dll.init();
}

static HRESULT ekt_io4_poll(void *ctx, struct io4_state *state)
{
    uint8_t opbtn;
    uint16_t x, y;
    uint32_t gamebtn;
    HRESULT hr;

    assert(ekt_dll.poll != NULL);
    assert(ekt_dll.get_opbtns != NULL);
    assert(ekt_dll.get_gamebtns != NULL);
    assert(ekt_dll.get_trackball_position != NULL);

    memset(state, 0, sizeof(*state));

    hr = ekt_dll.poll();

    if (FAILED(hr)) {
        return hr;
    }

    opbtn = 0;
    gamebtn = 0;
    x = 0;
    y = 0;

    ekt_dll.get_opbtns(&opbtn);
    ekt_dll.get_gamebtns(&gamebtn);
    ekt_dll.get_trackball_position(&x, &y);

    if (opbtn & EKT_IO_OPBTN_TEST) {
        state->buttons[0] |= IO4_BUTTON_TEST;
    }

    if (opbtn & EKT_IO_OPBTN_SERVICE) {
        state->buttons[0] |= IO4_BUTTON_SERVICE;
    }

    if (opbtn & EKT_IO_OPBTN_SW1) {
        state->buttons[0] |= 1 << 2;
    }

    if (opbtn & EKT_IO_OPBTN_SW2) {
        state->buttons[0] |= 1 << 3;
    }

    if (opbtn & EKT_IO_OPBTN_COIN) {
        coins++;
    }
    state->chutes[0] = coins << 8;

    if (!io_is_terminal) {
        if (gamebtn & EKT_IO_GAMEBTN_HOUGU) {
            state->buttons[1] |= 1 << 14;
        }

        if (gamebtn & EKT_IO_GAMEBTN_RYUUHA) {
            state->buttons[1] |= 1 << 12;
        }

        if (gamebtn & EKT_IO_GAMEBTN_MENU) {
            state->buttons[0] |= 1 << 13;
        }

        if (gamebtn & EKT_IO_GAMEBTN_START) {
            state->buttons[0] |= 1 << 7;
        }

        if (gamebtn & EKT_IO_GAMEBTN_STRATAGEM) {
            state->buttons[1] |= 1 << 15;
        }

        if (gamebtn & EKT_IO_GAMEBTN_STRATAGEM_LOCK) {
            state->buttons[1] |= 1 << 13;
        }

        if (gamebtn & EKT_IO_GAMEBTN_VOL_DOWN) {
            state->buttons[1] |= 1 << 10;
        }

        if (gamebtn & EKT_IO_GAMEBTN_VOL_UP) {
            state->buttons[1] |= 1 << 11;
        }
    }

    if (gamebtn & EKT_IO_GAMEBTN_NUMPAD_0) {
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_C2 : EKT_NUMPAD_TERM_C2;
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_R4 : EKT_NUMPAD_TERM_R4;
    }

    if (gamebtn & EKT_IO_GAMEBTN_NUMPAD_1) {
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_C1 : EKT_NUMPAD_TERM_C1;
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_R1 : EKT_NUMPAD_TERM_R1;
    }

    if (gamebtn & EKT_IO_GAMEBTN_NUMPAD_2) {
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_C2 : EKT_NUMPAD_TERM_C2;
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_R1 : EKT_NUMPAD_TERM_R1;
    }

    if (gamebtn & EKT_IO_GAMEBTN_NUMPAD_3) {
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_C3 : EKT_NUMPAD_TERM_C3;
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_R1 : EKT_NUMPAD_TERM_R1;
    }

    if (gamebtn & EKT_IO_GAMEBTN_NUMPAD_4) {
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_C1 : EKT_NUMPAD_TERM_C1;
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_R2 : EKT_NUMPAD_TERM_R2;
    }

    if (gamebtn & EKT_IO_GAMEBTN_NUMPAD_5) {
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_C2 : EKT_NUMPAD_TERM_C2;
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_R2 : EKT_NUMPAD_TERM_R2;
    }

    if (gamebtn & EKT_IO_GAMEBTN_NUMPAD_6) {
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_C3 : EKT_NUMPAD_TERM_C3;
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_R2 : EKT_NUMPAD_TERM_R2;
    }

    if (gamebtn & EKT_IO_GAMEBTN_NUMPAD_7) {
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_C1 : EKT_NUMPAD_TERM_C1;
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_R3 : EKT_NUMPAD_TERM_R3;
    }

    if (gamebtn & EKT_IO_GAMEBTN_NUMPAD_8) {
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_C2 : EKT_NUMPAD_TERM_C2;
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_R3 : EKT_NUMPAD_TERM_R3;
    }

    if (gamebtn & EKT_IO_GAMEBTN_NUMPAD_9) {
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_C3 : EKT_NUMPAD_TERM_C3;
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_R3 : EKT_NUMPAD_TERM_R3;
    }

    if (gamebtn & EKT_IO_GAMEBTN_NUMPAD_CLEAR) {
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_C1 : EKT_NUMPAD_TERM_C1;
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_R4 : EKT_NUMPAD_TERM_R4;
    }

    if (gamebtn & EKT_IO_GAMEBTN_NUMPAD_ENTER) {
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_C3 : EKT_NUMPAD_TERM_C3;
        state->buttons[0] |= !io_is_terminal ? EKT_NUMPAD_SATE_R4 : EKT_NUMPAD_TERM_R4;
    }

    if (io_is_terminal) {
        if (gamebtn & EKT_IO_GAMEBTN_TERMINAL_CANCEL) {
            state->buttons[1] |= 1 << 0;
        }

        if (gamebtn & EKT_IO_GAMEBTN_TERMINAL_DECIDE) {
            state->buttons[1] |= 1 << 1;
        }

        if (gamebtn & EKT_IO_GAMEBTN_TERMINAL_LEFT) {
            state->buttons[0] |= 1 << 3;
        }

        if (gamebtn & EKT_IO_GAMEBTN_TERMINAL_UP) {
            state->buttons[0] |= 1 << 5;
        }

        if (gamebtn & EKT_IO_GAMEBTN_TERMINAL_RIGHT) {
            state->buttons[0] |= 1 << 2;
        }

        if (gamebtn & EKT_IO_GAMEBTN_TERMINAL_DOWN) {
            state->buttons[0] |= 1 << 4;
        }

        if (gamebtn & EKT_IO_GAMEBTN_TERMINAL_LEFT_2) {
            state->buttons[1] |= 1 << 3;
        }

        if (gamebtn & EKT_IO_GAMEBTN_TERMINAL_RIGHT_2) {
            state->buttons[1] |= 1 << 2;
        }
    }

    state->spinners[2] = x;
    state->spinners[3] = y;

    return S_OK;
}
