#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "board/io4.h"

#include "soulhook/soul-dll.h"

#include "util/dprintf.h"

static HRESULT soul_io4_poll(void *ctx, struct io4_state *state);
static uint16_t coins;

static const struct io4_ops soul_io4_ops = {
    .poll = soul_io4_poll,
};

HRESULT soul_io4_hook_init(const struct io4_config *cfg)
{
    HRESULT hr;

    assert(soul_dll.init != NULL);

    hr = io4_hook_init(cfg, &soul_io4_ops, NULL, L"WGL", false);

    if (FAILED(hr)) {
        return hr;
    }

    return soul_dll.init();
}

static HRESULT soul_io4_poll(void *ctx, struct io4_state *state)
{
    uint8_t opbtn;
    uint8_t gamebtn;
    int16_t stick_x;
    int16_t stick_y;
    int16_t view_x;
    int16_t view_y;
    HRESULT hr;

    assert(soul_dll.poll != NULL);
    assert(soul_dll.get_opbtns != NULL);
    assert(soul_dll.get_gamebtns != NULL);
    assert(soul_dll.get_analogs != NULL);

    memset(state, 0, sizeof(*state));

    hr = soul_dll.poll();

    if (FAILED(hr)) {
        return hr;
    }

    opbtn = 0;
    gamebtn = 0;
    stick_x = 0;
    stick_y = 0;
    view_x = 0;
    view_y = 0;

    soul_dll.get_opbtns(&opbtn);
    soul_dll.get_gamebtns(&gamebtn);
    soul_dll.get_analogs(&stick_x, &stick_y, &view_x, &view_y);

    if (opbtn & SOUL_IO_OPBTN_TEST) {
        state->buttons[0] |= IO4_BUTTON_TEST;
    }

    if (opbtn & SOUL_IO_OPBTN_SERVICE) {
        state->buttons[0] |= IO4_BUTTON_SERVICE;
    }

    if (opbtn & SOUL_IO_OPBTN_COIN) {
        coins++;
    }
    state->chutes[0] = coins << 8;

    if (gamebtn & SOUL_IO_STICK_PUSH) {
        state->buttons[0] |= 1 << 3;
    }

    if (gamebtn & SOUL_IO_GAMEBTN_L1) {
        state->buttons[0] |= 1 << 7;
    }

    if (gamebtn & SOUL_IO_GAMEBTN_L2) {
        state->buttons[0] |= 1 << 2;
    }

    if (gamebtn & SOUL_IO_VIEW_STICK_PUSH) {
        state->buttons[1] |= 1 << 5;
    }

    if (gamebtn & SOUL_IO_GAMEBTN_A) {
        state->buttons[1] |= 1 << 7;
    }

    if (gamebtn & SOUL_IO_GAMEBTN_B) {
        state->buttons[1] |= 1 << 2;
    }

    if (gamebtn & SOUL_IO_GAMEBTN_C) {
        state->buttons[1] |= 1 << 3;
    }

    /* Stick x and y movement */

    state->adcs[0] = 0x8000 + stick_x;
    state->adcs[4] = 0x8000 + stick_y;

    state->adcs[1] = 0x8000 + view_x;
    state->adcs[5] = 0x8000 + view_y;

    return S_OK;
}
