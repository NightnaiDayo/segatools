#include <windows.h>

#include <limits.h>
#include <stdint.h>
#include <assert.h>
#include <util/dprintf.h>

#include "kemonoio/kemonoio.h"
#include "kemonoio/config.h"
#include "util/env.h"

static uint8_t kemono_opbtn;
static uint16_t kemono_pbtn;
static uint16_t kemono_io_coins;
static struct kemono_io_config kemono_io_cfg;
static bool kemono_io_coin;

uint16_t kemono_io_get_api_version(void) {
    return 0x0100;
}

HRESULT kemono_io_init(void) {
    kemono_io_config_load(&kemono_io_cfg, get_config_path());

    kemono_io_coins = 0;

    return S_OK;
}

HRESULT kemono_io_poll(uint16_t *ops, uint16_t *player) {
    kemono_opbtn = 0;
    kemono_pbtn = 0;

    if (GetAsyncKeyState(kemono_io_cfg.vk_test) & 0x8000) {
        kemono_opbtn |= KEMONO_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(kemono_io_cfg.vk_service) & 0x8000) {
        kemono_opbtn |= KEMONO_IO_OPBTN_SERVICE;
    }

    if (kemono_io_cfg.vk_coin &&
        (GetAsyncKeyState(kemono_io_cfg.vk_coin) & 0x8000)) {
        if (!kemono_io_coin) {
            kemono_io_coin = true;
            kemono_io_coins++;
        }
    } else {
        kemono_io_coin = false;
    }

    if (GetAsyncKeyState(kemono_io_cfg.vk_up)) {
        kemono_pbtn |= KEMONO_IO_GAMEBTN_UP;
    }

    if (GetAsyncKeyState(kemono_io_cfg.vk_down)) {
        kemono_pbtn |= KEMONO_IO_GAMEBTN_DOWN;
    }

    if (GetAsyncKeyState(kemono_io_cfg.vk_left)) {
        kemono_pbtn |= KEMONO_IO_GAMEBTN_LEFT;
    }

    if (GetAsyncKeyState(kemono_io_cfg.vk_right)) {
        kemono_pbtn |= KEMONO_IO_GAMEBTN_RIGHT;
    }

    if (GetAsyncKeyState(kemono_io_cfg.vk_red)) {
        kemono_pbtn |= KEMONO_IO_GAMEBTN_R;
    }

    if (GetAsyncKeyState(kemono_io_cfg.vk_green)) {
        kemono_pbtn |= KEMONO_IO_GAMEBTN_G;
    }

    if (GetAsyncKeyState(kemono_io_cfg.vk_blue)) {
        kemono_pbtn |= KEMONO_IO_GAMEBTN_B;
    }

    if (GetAsyncKeyState(kemono_io_cfg.vk_start)) {
        kemono_pbtn |= KEMONO_IO_GAMEBTN_START;
    }

    if (ops != NULL) {
        *ops = kemono_opbtn;
    }
    if (player != NULL) {
        *player = kemono_pbtn;
    }

    return S_OK;
}

void kemono_io_jvs_read_coin_counter(uint16_t *out) {
    assert(out != NULL);

    *out = kemono_io_coins;
}

HRESULT kemono_io_led_init(void) {
    return S_OK;
}

void kemono_io_led_set_colors(uint8_t board, uint8_t *rgb) {

}

void kemono_io_jvs_write_gpio(uint32_t state){

}
