#include <windows.h>
#include <xinput.h>
#include <math.h>

#include <limits.h>
#include <stdint.h>

#include "apm3io/apm3io.h"
#include "apm3io/config.h"
#include "util/dprintf.h"
#include "util/env.h"

static uint8_t apm3_opbtn;
static uint32_t apm3_gamebtn;
static int16_t apm3_stick_x;
static int16_t apm3_stick_y;
static struct apm3_io_config apm3_io_cfg;
static bool apm3_io_coin;

uint16_t apm3_io_get_api_version(void) {
    return 0x0100;
}

HRESULT apm3_io_init(void) {
    apm3_io_config_load(&apm3_io_cfg, get_config_path());

    return S_OK;
}

HRESULT apm3_io_poll(void) {
    apm3_opbtn = 0;
    apm3_gamebtn = 0;

    if (GetAsyncKeyState(apm3_io_cfg.vk_test) & 0x8000) {
        apm3_opbtn |= APM3_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_service) & 0x8000) {
        apm3_opbtn |= APM3_IO_OPBTN_SERVICE;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_coin) & 0x8000) {
        if (!apm3_io_coin) {
            apm3_io_coin = true;
            apm3_opbtn |= APM3_IO_OPBTN_COIN;
        }
    } else {
        apm3_io_coin = false;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_home) & 0x8000) {
        apm3_gamebtn |= APM3_IO_GAMEBTN_HOME;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_start) & 0x8000) {
        apm3_gamebtn |= APM3_IO_GAMEBTN_START;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_up) & 0x8000) {
        apm3_gamebtn |= APM3_IO_GAMEBTN_UP;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_right) & 0x8000) {
        apm3_gamebtn |= APM3_IO_GAMEBTN_RIGHT;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_down) & 0x8000) {
        apm3_gamebtn |= APM3_IO_GAMEBTN_DOWN;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_left) & 0x8000) {
        apm3_gamebtn |= APM3_IO_GAMEBTN_LEFT;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_buttons[0]) & 0x8000) {
        apm3_gamebtn |= APM3_IO_GAMEBTN_B1;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_buttons[1]) & 0x8000) {
        apm3_gamebtn |= APM3_IO_GAMEBTN_B2;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_buttons[2]) & 0x8000) {
        apm3_gamebtn |= APM3_IO_GAMEBTN_B3;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_buttons[3]) & 0x8000) {
        apm3_gamebtn |= APM3_IO_GAMEBTN_B4;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_buttons[4]) & 0x8000) {
        apm3_gamebtn |= APM3_IO_GAMEBTN_B5;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_buttons[5]) & 0x8000) {
        apm3_gamebtn |= APM3_IO_GAMEBTN_B6;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_buttons[6]) & 0x8000) {
        apm3_gamebtn |= APM3_IO_GAMEBTN_B7;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_buttons[7]) & 0x8000) {
        apm3_gamebtn |= APM3_IO_GAMEBTN_B8;
    }

    return S_OK;
}

void apm3_io_get_opbtns(uint8_t* opbtn) {
    if (opbtn != NULL) {
        *opbtn = apm3_opbtn;
    }
}

void apm3_io_get_gamebtns(uint32_t* btn) {
    if (btn != NULL) {
        *btn = apm3_gamebtn;
    }
}

HRESULT apm3_io_led_init(void) {
    return S_OK;
}

void apm3_io_led_set_colors(uint8_t board, uint8_t* rgb) {
    return;
}
