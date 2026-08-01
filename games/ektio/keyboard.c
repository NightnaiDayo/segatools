#include <windows.h>

#include <math.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>

#include "ektio/backend.h"
#include "ektio/config.h"
#include "ektio/ektio.h"
#include "ektio/keyboard.h"

#include "util/dprintf.h"

static void ekt_kb_get_gamebtns(uint32_t* gamebtn_out);
static void ekt_kb_get_trackball(uint16_t* x, uint16_t* y);

static const struct ekt_io_backend ekt_kb_backend = {
    .get_gamebtns = ekt_kb_get_gamebtns,
    .get_trackball = ekt_kb_get_trackball
};

static uint16_t current_x;
static uint16_t current_y;

static struct ekt_kb_config config;

HRESULT ekt_kb_init(const struct ekt_kb_config* cfg, const struct ekt_io_backend** backend) {
    assert(cfg != NULL);
    assert(backend != NULL);

    dprintf("Keyboard: Using keyboard input\n");
    *backend = &ekt_kb_backend;
    config = *cfg;

    return S_OK;
}

static void ekt_kb_get_gamebtns(uint32_t* gamebtn_out) {
    assert(gamebtn_out != NULL);

    uint32_t gamebtn = 0;

    if (GetAsyncKeyState(config.vk_hougu) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_HOUGU;
    }

    if (GetAsyncKeyState(config.vk_menu) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_MENU;
    }

    if (GetAsyncKeyState(config.vk_start) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_START;
    }

    if (GetAsyncKeyState(config.vk_stratagem) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_STRATAGEM;
    }

    if (GetAsyncKeyState(config.vk_stratagem_lock) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_STRATAGEM_LOCK;
    }

    if (GetAsyncKeyState(config.vk_ryuuha) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_RYUUHA;
    }

    if (GetAsyncKeyState(config.vk_tenkey_0) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_NUMPAD_0;
    }

    if (GetAsyncKeyState(config.vk_tenkey_1) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_NUMPAD_1;
    }

    if (GetAsyncKeyState(config.vk_tenkey_2) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_NUMPAD_2;
    }

    if (GetAsyncKeyState(config.vk_tenkey_3) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_NUMPAD_3;
    }

    if (GetAsyncKeyState(config.vk_tenkey_4) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_NUMPAD_4;
    }

    if (GetAsyncKeyState(config.vk_tenkey_5) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_NUMPAD_5;
    }

    if (GetAsyncKeyState(config.vk_tenkey_6) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_NUMPAD_6;
    }

    if (GetAsyncKeyState(config.vk_tenkey_7) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_NUMPAD_7;
    }

    if (GetAsyncKeyState(config.vk_tenkey_8) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_NUMPAD_8;
    }

    if (GetAsyncKeyState(config.vk_tenkey_9) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_NUMPAD_9;
    }

    if (GetAsyncKeyState(config.vk_tenkey_clear) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_NUMPAD_CLEAR;
    }

    if (GetAsyncKeyState(config.vk_tenkey_enter) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_NUMPAD_ENTER;
    }

    if (GetAsyncKeyState(config.vk_vol_down) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_VOL_DOWN;
    }

    if (GetAsyncKeyState(config.vk_vol_up) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_VOL_UP;
    }

    if (GetAsyncKeyState(config.vk_terminal_cancel) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_TERMINAL_CANCEL;
    }

    if (GetAsyncKeyState(config.vk_terminal_decide) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_TERMINAL_DECIDE;
    }

    if (GetAsyncKeyState(config.vk_terminal_up) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_TERMINAL_UP;
    }

    if (GetAsyncKeyState(config.vk_terminal_right) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_TERMINAL_RIGHT;
    }

    if (GetAsyncKeyState(config.vk_terminal_down) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_TERMINAL_DOWN;
    }

    if (GetAsyncKeyState(config.vk_terminal_left) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_TERMINAL_LEFT;
    }

    if (GetAsyncKeyState(config.vk_terminal_left_2) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_TERMINAL_LEFT_2;
    }

    if (GetAsyncKeyState(config.vk_terminal_right_2) & 0x8000) {
        gamebtn |= EKT_IO_GAMEBTN_TERMINAL_RIGHT_2;
    }

    *gamebtn_out = gamebtn;
}

static void ekt_kb_get_trackball(uint16_t* x, uint16_t* y) {
    assert(x != NULL);
    assert(y != NULL);

    if (GetAsyncKeyState(config.x_down) & 0x8000) {
        current_x -= config.speed;
    } else if (GetAsyncKeyState(config.x_up) & 0x8000) {
        current_x += config.speed;
    }
    if (GetAsyncKeyState(config.y_down) & 0x8000) {
        current_y += config.speed;
    } else if (GetAsyncKeyState(config.y_up) & 0x8000) {
        current_y -= config.speed;
    }

    *x = current_x;
    *y = current_y;
}
