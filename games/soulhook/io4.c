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

    hr = io4_hook_init(cfg, &soul_io4_ops, NULL, L"Soul Reverse", false);

    if (FAILED(hr)) {
        return hr;
    }

    return soul_dll.init();
}

static HRESULT soul_io4_poll(void *ctx, struct io4_state *state)
{
    uint8_t opbtn;
    uint8_t left;
    uint8_t right;
    int16_t lever;
    HRESULT hr;

    assert(soul_dll.poll != NULL);
    assert(soul_dll.get_opbtns != NULL);
    assert(soul_dll.get_gamebtns != NULL);

    memset(state, 0, sizeof(*state));

    hr = soul_dll.poll();

    if (FAILED(hr)) {
        return hr;
    }

    opbtn = 0;
    left = 0;
    right = 0;
    lever = 0;

    soul_dll.get_opbtns(&opbtn);
    soul_dll.get_gamebtns(&left, &right);

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

    if (left & SOUL_IO_GAMEBTN_A) {
        state->buttons[0] |= 1 << 0;
    }

    if (left & SOUL_IO_GAMEBTN_B) {
        state->buttons[0] |= 1 << 5;
    }

    if (left & SOUL_IO_GAMEBTN_C) {
        state->buttons[0] |= 1 << 7;
    }

    return S_OK;
}
