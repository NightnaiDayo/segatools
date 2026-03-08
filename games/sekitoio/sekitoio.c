#include <windows.h>
#include <xinput.h>
#include <math.h>

#include <stdint.h>

#include "sekitoio/sekitoio.h"

#include <assert.h>

#include "keyboard.h"
#include "sekitoio/config.h"
#include "util/dprintf.h"
#include "util/env.h"
#include "util/str.h"

static uint8_t sekito_opbtn;
static uint32_t sekito_gamebtn;
static uint8_t sekito_stick_x;
static uint8_t sekito_stick_y;
static struct sekito_io_config sekito_io_cfg;
static const struct sekito_io_backend* sekito_io_backend;
static bool sekito_io_coin;

uint16_t sekito_io_get_api_version(void) {
    return 0x0100;
}

HRESULT sekito_io_init(void) {
    sekito_io_config_load(&sekito_io_cfg, get_config_path());

    HRESULT hr;

    if (wstr_ieq(sekito_io_cfg.mode, L"keyboard")) {
        hr = sekito_kb_init(&sekito_io_cfg.kb, &sekito_io_backend);
    } else {
        hr = E_INVALIDARG;
        dprintf("Sekito IO: Invalid IO mode \"%S\", use keyboard\n",
                sekito_io_cfg.mode);
    }

    return hr;
}

HRESULT sekito_io_poll(void) {
    assert(sekito_io_backend != NULL);

    sekito_opbtn = 0;
    sekito_gamebtn = 0;
    sekito_stick_x = 0;
    sekito_stick_y = 0;

    if (GetAsyncKeyState(sekito_io_cfg.vk_test) & 0x8000) {
        sekito_opbtn |= SEKITO_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(sekito_io_cfg.vk_service) & 0x8000) {
        sekito_opbtn |= SEKITO_IO_OPBTN_SERVICE;
    }

    if (GetAsyncKeyState(sekito_io_cfg.vk_sw1) & 0x8000) {
        sekito_opbtn |= SEKITO_IO_OPBTN_SW1;
    }

    if (GetAsyncKeyState(sekito_io_cfg.vk_sw2) & 0x8000) {
        sekito_opbtn |= SEKITO_IO_OPBTN_SW2;
    }

    if (GetAsyncKeyState(sekito_io_cfg.vk_coin) & 0x8000) {
        if (!sekito_io_coin) {
            sekito_io_coin = true;
            sekito_opbtn |= SEKITO_IO_OPBTN_COIN;
        }
    } else {
        sekito_io_coin = false;
    }

    return S_OK;
}

void sekito_io_get_opbtns(uint8_t* opbtn) {
    if (opbtn != NULL) {
        *opbtn = sekito_opbtn;
    }
}

void sekito_io_get_gamebtns(uint32_t* btn) {
    assert(sekito_io_backend != NULL);
    assert(btn != NULL);

    sekito_io_backend->get_gamebtns(btn);
}

void sekito_io_get_trackball_position(uint16_t* stick_x, uint16_t* stick_y) {
    assert(sekito_io_backend != NULL);
    assert(stick_x != NULL);
    assert(stick_y != NULL);

    sekito_io_backend->get_trackball(stick_x, stick_y);
}

HRESULT sekito_io_led_init(void) {
    return S_OK;
}

void sekito_io_led_set_colors(uint8_t board, uint8_t* rgb) {
    return;
}
