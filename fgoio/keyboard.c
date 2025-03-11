#include <windows.h>

#include <math.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>

#include "fgoio/backend.h"
#include "fgoio/config.h"
#include "fgoio/fgoio.h"
#include "fgoio/keyboard.h"

#include "util/dprintf.h"

static void fgo_kb_get_gamebtns(uint8_t* gamebtn_out);
static void fgo_kb_get_analogs(int16_t* x, int16_t* y);

static const struct fgo_io_backend fgo_kb_backend = {
    .get_gamebtns = fgo_kb_get_gamebtns,
    .get_analogs = fgo_kb_get_analogs
};

static struct fgo_kb_config config;

HRESULT fgo_kb_init(const struct fgo_kb_config* cfg, const struct fgo_io_backend** backend) {
    assert(cfg != NULL);
    assert(backend != NULL);

    dprintf("Keyboard: Using keyboard input\n");
    *backend = &fgo_kb_backend;
    config = *cfg;

    return S_OK;
}

static void fgo_kb_get_gamebtns(uint8_t* gamebtn_out) {
    assert(gamebtn_out != NULL);

    uint8_t gamebtn = 0;

    if (GetAsyncKeyState(config.vk_np) & 0x8000) {
        gamebtn |= FGO_IO_GAMEBTN_NOBLE_PHANTASM;
    }

    if (GetAsyncKeyState(config.vk_target) & 0x8000) {
        gamebtn |= FGO_IO_GAMEBTN_TARGET;
    }

    if (GetAsyncKeyState(config.vk_dash) & 0x8000) {
        gamebtn |= FGO_IO_GAMEBTN_SPEED_UP;
    }

    if (GetAsyncKeyState(config.vk_attack) & 0x8000) {
        gamebtn |= FGO_IO_GAMEBTN_ATTACK;
    }

    if (GetAsyncKeyState(config.vk_camera) & 0x8000) {
        gamebtn |= FGO_IO_GAMEBTN_CAMERA;
    }

    *gamebtn_out = gamebtn;
}

static void fgo_kb_get_analogs(int16_t* x, int16_t* y) {
    assert(x != NULL);
    assert(y != NULL);

    if (GetAsyncKeyState(config.vk_left) & 0x8000) {
        *x = SHRT_MIN + 1;
    } else if (GetAsyncKeyState(config.vk_right) & 0x8000) {
        *x = SHRT_MAX - 1;
    } else {
        *x = 0;
    }
    if (GetAsyncKeyState(config.vk_down) & 0x8000) {
        *y = SHRT_MIN + 1;
    } else if (GetAsyncKeyState(config.vk_up) & 0x8000) {
        *y = SHRT_MAX - 1;
    } else {
        *y = 0;
    }
}
