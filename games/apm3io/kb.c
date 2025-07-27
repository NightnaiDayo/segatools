#include <windows.h>
#include <xinput.h>
#include <math.h>

#include <assert.h>
#include <limits.h>
#include <stdint.h>

#include "apm3io/apm3io.h"
#include "apm3io/config.h"
#include "apm3io/kb.h"

#include "util/dprintf.h"
#include "util/env.h"

static uint8_t apm3_kb_home;
static uint8_t apm3_kb_start;
static uint8_t apm3_kb_up;
static uint8_t apm3_kb_right;
static uint8_t apm3_kb_down;
static uint8_t apm3_kb_left;
static uint8_t apm3_kb_button[8];

static void apm3_kb_get_gamebtns(uint16_t *gamebtn_out);

static HRESULT apm3_kb_config_apply(const struct apm3_kb_config *cfg);

static const struct apm3_io_backend apm3_kb_backend = {
    .get_gamebtns = apm3_kb_get_gamebtns,
};

HRESULT apm3_kb_init(const struct apm3_kb_config *cfg,
                     const struct apm3_io_backend **backend) {
    HRESULT hr;

    assert(cfg != NULL);
    assert(backend != NULL);

    *backend = NULL;

    hr = apm3_kb_config_apply(cfg);

    if (FAILED(hr)) {
        return hr;
    }

    dprintf("Keyboard: Using keyboard input\n");
    *backend = &apm3_kb_backend;

    return S_OK;
}

static HRESULT apm3_kb_config_apply(const struct apm3_kb_config *cfg) {
    int i;

    if (cfg->vk_start > 255) {
        dprintf("Keyboard: Invalid start key configuration: %u\n", cfg->vk_start);
        return E_INVALIDARG;
    }

    if (cfg->vk_home > 255) {
        dprintf("Keyboard: Invalid home key configuration: %u\n", cfg->vk_home);
        return E_INVALIDARG;
    }

    if (cfg->vk_up > 255) {
        dprintf("Keyboard: Invalid up key configuration: %u\n", cfg->vk_up);
        return E_INVALIDARG;
    }

    if (cfg->vk_right > 255) {
        dprintf("Keyboard: Invalid right key configuration: %u\n", cfg->vk_right);
        return E_INVALIDARG;
    }

    if (cfg->vk_down > 255) {
        dprintf("Keyboard: Invalid down key configuration: %u\n", cfg->vk_down);
        return E_INVALIDARG;
    }

    if (cfg->vk_left > 255) {
        dprintf("Keyboard: Invalid left key configuration: %u\n", cfg->vk_left);
        return E_INVALIDARG;
    }

    for (i = 0; i < APM3_BUTTON_COUNT; i++) {
        if (cfg->vk_buttons[i] > 255) {
            dprintf("Keyboard: Invalid button %i configuration\n", i);
            return E_INVALIDARG;
        }
        apm3_kb_button[i] = cfg->vk_buttons[i];
    }

    /* Apply the configuration */
    apm3_kb_home = cfg->vk_home;
    apm3_kb_start = cfg->vk_start;
    apm3_kb_up = cfg->vk_up;
    apm3_kb_right = cfg->vk_right;
    apm3_kb_down = cfg->vk_down;
    apm3_kb_left = cfg->vk_left;

    /* Print some debug output to make sure config works... */
    dprintf("Keyboard: --- Begin configuration ---\n");
    dprintf("Keyboard: Home key . . . . : %u\n", apm3_kb_home);
    dprintf("Keyboard: Start key  . . . : %u\n", apm3_kb_start);

    dprintf("Keyboard: Down key . . . . : %u\n", cfg->vk_down);
    dprintf("Keyboard: Left key . . . . : %u\n", cfg->vk_left);

    /* Print the configuration for all 8 buttons */
    for (i = 0; i < APM3_BUTTON_COUNT; i++) {
        dprintf("Keyboard: Button %i . . . . : %u\n", i + 1, apm3_kb_button[i]);
    }

    dprintf("Keyboard: --- End configuration ---\n");

    return S_OK;
}

static void apm3_kb_get_gamebtns(uint16_t* gamebtn_out) {
    uint16_t gamebtn;

    assert(gamebtn_out != NULL);

    gamebtn = 0;

    if (GetAsyncKeyState(apm3_kb_home) & 0x8000) {
        gamebtn |= APM3_IO_GAMEBTN_HOME;
    }

    if (GetAsyncKeyState(apm3_kb_start) & 0x8000) {
        gamebtn |= APM3_IO_GAMEBTN_START;
    }

    if (GetAsyncKeyState(apm3_kb_up) & 0x8000) {
        gamebtn |= APM3_IO_GAMEBTN_UP;
    }

    if (GetAsyncKeyState(apm3_kb_right) & 0x8000) {
        gamebtn |= APM3_IO_GAMEBTN_RIGHT;
    }

    if (GetAsyncKeyState(apm3_kb_down) & 0x8000) {
        gamebtn |= APM3_IO_GAMEBTN_DOWN;
    }

    if (GetAsyncKeyState(apm3_kb_left) & 0x8000) {
        gamebtn |= APM3_IO_GAMEBTN_LEFT;
    }

    if (GetAsyncKeyState(apm3_kb_button[0]) & 0x8000) {
        gamebtn |= APM3_IO_GAMEBTN_B1;
    }

    if (GetAsyncKeyState(apm3_kb_button[1]) & 0x8000) {
        gamebtn |= APM3_IO_GAMEBTN_B2;
    }

    if (GetAsyncKeyState(apm3_kb_button[2]) & 0x8000) {
        gamebtn |= APM3_IO_GAMEBTN_B3;
    }

    if (GetAsyncKeyState(apm3_kb_button[3]) & 0x8000) {
        gamebtn |= APM3_IO_GAMEBTN_B4;
    }

    if (GetAsyncKeyState(apm3_kb_button[4]) & 0x8000) {
        gamebtn |= APM3_IO_GAMEBTN_B5;
    }

    if (GetAsyncKeyState(apm3_kb_button[5]) & 0x8000) {
        gamebtn |= APM3_IO_GAMEBTN_B6;
    }

    if (GetAsyncKeyState(apm3_kb_button[6]) & 0x8000) {
        gamebtn |= APM3_IO_GAMEBTN_B7;
    }

    if (GetAsyncKeyState(apm3_kb_button[7]) & 0x8000) {
        gamebtn |= APM3_IO_GAMEBTN_B8;
    }

    *gamebtn_out = gamebtn;
}
