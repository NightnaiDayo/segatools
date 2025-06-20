#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "board/io4.h"

#include "fgohook/fgo-dll.h"

#include "util/dprintf.h"

static HRESULT fgo_io4_poll(void *ctx, struct io4_state *state);
static uint16_t coins;

static const struct io4_ops fgo_io4_ops = {
    .poll = fgo_io4_poll,
};

HRESULT fgo_io4_hook_init(const struct io4_config *cfg)
{
    HRESULT hr;

    assert(fgo_dll.init != NULL);

    hr = io4_hook_init(cfg, &fgo_io4_ops, NULL);

    if (FAILED(hr)) {
        return hr;
    }

    return fgo_dll.init();
}

static HRESULT fgo_io4_poll(void *ctx, struct io4_state *state)
{
    uint8_t opbtn;
    uint8_t gamebtn;
    int16_t stick_x;
    int16_t stick_y;
    HRESULT hr;

    assert(fgo_dll.poll != NULL);
    assert(fgo_dll.get_opbtns != NULL);
    assert(fgo_dll.get_gamebtns != NULL);
    assert(fgo_dll.get_analogs != NULL);

    memset(state, 0, sizeof(*state));

    hr = fgo_dll.poll();

    if (FAILED(hr)) {
        return hr;
    }

    opbtn = 0;
    gamebtn = 0;
    stick_x = 0;
    stick_y = 0;

    fgo_dll.get_opbtns(&opbtn);
    fgo_dll.get_gamebtns(&gamebtn);
    fgo_dll.get_analogs(&stick_x, &stick_y);

    if (opbtn & FGO_IO_OPBTN_TEST) {
        state->buttons[0] |= IO4_BUTTON_TEST;
    }

    if (opbtn & FGO_IO_OPBTN_SERVICE) {
        state->buttons[0] |= IO4_BUTTON_SERVICE;
    }

    if (opbtn & FGO_IO_OPBTN_COIN) {
        coins++;
    }
    state->chutes[0] = coins << 8;

    if (gamebtn & FGO_IO_GAMEBTN_SPEED_UP) {
        state->buttons[0] |= 1 << 4;
    }

    if (gamebtn & FGO_IO_GAMEBTN_TARGET) {
        state->buttons[0] |= 1 << 5;
    }

    if (gamebtn & FGO_IO_GAMEBTN_ATTACK) {
        state->buttons[0] |= 1 << 1;
    }

    if (gamebtn & FGO_IO_GAMEBTN_NOBLE_PHANTASM) {
        state->buttons[0] |= 1 << 0;
    }

    if (gamebtn & FGO_IO_GAMEBTN_CAMERA) {
        state->buttons[0] |= 1 << 14;
    }

    /* Stick x and y movement */

    state->adcs[0] = 0x8000 - stick_x;
    state->adcs[4] = 0x8000 + stick_y;

    return S_OK;
}
