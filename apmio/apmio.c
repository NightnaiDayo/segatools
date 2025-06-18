#include <windows.h>
#include <xinput.h>
#include <math.h>

#include <limits.h>
#include <stdint.h>

#include "apmio/apmio.h"
#include "apmio/config.h"
#include "util/dprintf.h"
#include "util/env.h"

static uint8_t apm_opbtn;
static uint32_t apm_gamebtn;
static int16_t apm_stick_x;
static int16_t apm_stick_y;
static struct apm_io_config apm_io_cfg;
static bool apm_io_coin;

uint16_t apm_io_get_api_version(void) {
    return 0x0100;
}

HRESULT apm_io_init(void) {
    apm_io_config_load(&apm_io_cfg, get_config_path());

    return S_OK;
}

HRESULT apm_io_poll(void) {
    apm_opbtn = 0;
    apm_gamebtn = 0;

    if (GetAsyncKeyState(apm_io_cfg.vk_test) & 0x8000) {
        apm_opbtn |= APM_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_service) & 0x8000) {
        apm_opbtn |= APM_IO_OPBTN_SERVICE;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_coin) & 0x8000) {
        if (!apm_io_coin) {
            apm_io_coin = true;
            apm_opbtn |= APM_IO_OPBTN_COIN;
        }
    } else {
        apm_io_coin = false;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_home) & 0x8000) {
        apm_gamebtn |= APM_IO_GAMEBTN_HOME;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_start) & 0x8000) {
        apm_gamebtn |= APM_IO_GAMEBTN_START;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_up) & 0x8000) {
        apm_gamebtn |= APM_IO_GAMEBTN_UP;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_right) & 0x8000) {
        apm_gamebtn |= APM_IO_GAMEBTN_RIGHT;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_down) & 0x8000) {
        apm_gamebtn |= APM_IO_GAMEBTN_DOWN;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_left) & 0x8000) {
        apm_gamebtn |= APM_IO_GAMEBTN_LEFT;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_buttons[0]) & 0x8000) {
        apm_gamebtn |= APM_IO_GAMEBTN_B1;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_buttons[1]) & 0x8000) {
        apm_gamebtn |= APM_IO_GAMEBTN_B2;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_buttons[2]) & 0x8000) {
        apm_gamebtn |= APM_IO_GAMEBTN_B3;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_buttons[3]) & 0x8000) {
        apm_gamebtn |= APM_IO_GAMEBTN_B4;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_buttons[4]) & 0x8000) {
        apm_gamebtn |= APM_IO_GAMEBTN_B5;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_buttons[5]) & 0x8000) {
        apm_gamebtn |= APM_IO_GAMEBTN_B6;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_buttons[6]) & 0x8000) {
        apm_gamebtn |= APM_IO_GAMEBTN_B7;
    }

    if (GetAsyncKeyState(apm_io_cfg.vk_buttons[7]) & 0x8000) {
        apm_gamebtn |= APM_IO_GAMEBTN_B8;
    }

    return S_OK;
}

void apm_io_get_opbtns(uint8_t* opbtn) {
    if (opbtn != NULL) {
        *opbtn = apm_opbtn;
    }
}

void apm_io_get_gamebtns(uint32_t* btn) {
    if (btn != NULL) {
        *btn = apm_gamebtn;
    }
}

HRESULT apm_io_led_init(void) {
    return S_OK;
}

void apm_io_led_set_colors(uint8_t board, uint8_t* rgb) {
    return;
}
