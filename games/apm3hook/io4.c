#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "board/io4.h"

#include "apm3hook/apm3-dll.h"

#include "util/dprintf.h"

static HRESULT apm3_io4_poll(void *ctx, struct io4_state *state);
static uint16_t coins;

static const struct io4_ops apm3_io4_ops = {
    .poll = apm3_io4_poll,
};

HRESULT apm3_io4_hook_init(const struct io4_config *cfg)
{
    HRESULT hr;

    assert(apm3_dll.init != NULL);

    hr = io4_hook_init(cfg, &apm3_io4_ops, NULL);

    if (FAILED(hr)) {
        return hr;
    }

    return apm3_dll.init();
}

static HRESULT apm3_io4_poll(void *ctx, struct io4_state *state)
{
    uint8_t opbtn;
    uint32_t gamebtn;
    HRESULT hr;

    assert(apm3_dll.poll != NULL);
    assert(apm3_dll.get_opbtns != NULL);
    assert(apm3_dll.get_gamebtns != NULL);

    memset(state, 0, sizeof(*state));

    hr = apm3_dll.poll();

    if (FAILED(hr)) {
        return hr;
    }

    opbtn = 0;
    gamebtn = 0;

    apm3_dll.get_opbtns(&opbtn);
    apm3_dll.get_gamebtns(&gamebtn);

    if (opbtn & APM3_IO_OPBTN_TEST) {
        state->buttons[0] |= IO4_BUTTON_TEST;
    }

    if (opbtn & APM3_IO_OPBTN_SERVICE) {
        state->buttons[0] |= IO4_BUTTON_SERVICE;
    }

    if (opbtn & APM3_IO_OPBTN_COIN) {
        coins++;
    }
    state->chutes[0] = coins << 8;

    if (gamebtn & APM3_IO_GAMEBTN_HOME) {
        state->buttons[1] |= 1 << 12;
    }

    if (gamebtn & APM3_IO_GAMEBTN_START) {
        state->buttons[0] |= 1 << 7;
    }

    if (gamebtn & APM3_IO_GAMEBTN_UP) {
        state->buttons[0] |= 1 << 5;
    }

    if (gamebtn & APM3_IO_GAMEBTN_RIGHT) {
        state->buttons[0] |= 1 << 2;
    }

    if (gamebtn & APM3_IO_GAMEBTN_DOWN) {
        state->buttons[0] |= 1 << 4;
    }

    if (gamebtn & APM3_IO_GAMEBTN_LEFT) {
        state->buttons[0] |= 1 << 3;
    }

    if (gamebtn & APM3_IO_GAMEBTN_B1) {
        state->buttons[0] |= 1 << 1;
    }

    if (gamebtn & APM3_IO_GAMEBTN_B2) {
        state->buttons[0] |= 1 << 0;
    }

    if (gamebtn & APM3_IO_GAMEBTN_B3) {
        state->buttons[0] |= 1 << 15;
    }

    if (gamebtn & APM3_IO_GAMEBTN_B4) {
        state->buttons[0] |= 1 << 14;
    }

    if (gamebtn & APM3_IO_GAMEBTN_B5) {
        state->buttons[0] |= 1 << 13;
    }

    if (gamebtn & APM3_IO_GAMEBTN_B6) {
        state->buttons[0] |= 1 << 12;
    }

    if (gamebtn & APM3_IO_GAMEBTN_B7) {
        state->buttons[0] |= 1 << 11;
    }

    if (gamebtn & APM3_IO_GAMEBTN_B8) {
        state->buttons[0] |= 1 << 10;
    }

    return S_OK;
}
