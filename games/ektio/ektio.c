#include <windows.h>
#include <xinput.h>
#include <math.h>

#include <stdint.h>

#include "ektio/ektio.h"

#include <assert.h>

#include "keyboard.h"
#include "ektio/config.h"
#include "util/dprintf.h"
#include "util/env.h"
#include "util/str.h"

static uint8_t ekt_opbtn;
static uint32_t ekt_gamebtn;
static uint8_t ekt_stick_x;
static uint8_t ekt_stick_y;
static struct ekt_io_config ekt_io_cfg;
static const struct ekt_io_backend* ekt_io_backend;
static bool ekt_io_coin;

uint16_t ekt_io_get_api_version(void) {
    return 0x0100;
}

HRESULT ekt_io_init(void) {
    ekt_io_config_load(&ekt_io_cfg, get_config_path());

    HRESULT hr;

    if (wstr_ieq(ekt_io_cfg.mode, L"keyboard")) {
        hr = ekt_kb_init(&ekt_io_cfg.kb, &ekt_io_backend);
    } else {
        hr = E_INVALIDARG;
        dprintf("EKT IO: Invalid IO mode \"%S\", use keyboard\n",
                ekt_io_cfg.mode);
    }

    return hr;
}

HRESULT ekt_io_poll(void) {
    assert(ekt_io_backend != NULL);

    ekt_opbtn = 0;
    ekt_gamebtn = 0;
    ekt_stick_x = 0;
    ekt_stick_y = 0;

    if (GetAsyncKeyState(ekt_io_cfg.vk_test) & 0x8000) {
        ekt_opbtn |= EKT_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(ekt_io_cfg.vk_service) & 0x8000) {
        ekt_opbtn |= EKT_IO_OPBTN_SERVICE;
    }

    if (GetAsyncKeyState(ekt_io_cfg.vk_sw1) & 0x8000) {
        ekt_opbtn |= EKT_IO_OPBTN_SW1;
    }

    if (GetAsyncKeyState(ekt_io_cfg.vk_sw2) & 0x8000) {
        ekt_opbtn |= EKT_IO_OPBTN_SW2;
    }

    if (GetAsyncKeyState(ekt_io_cfg.vk_coin) & 0x8000) {
        if (!ekt_io_coin) {
            ekt_io_coin = true;
            ekt_opbtn |= EKT_IO_OPBTN_COIN;
        }
    } else {
        ekt_io_coin = false;
    }

    return S_OK;
}

void ekt_io_get_opbtns(uint8_t* opbtn) {
    if (opbtn != NULL) {
        *opbtn = ekt_opbtn;
    }
}

void ekt_io_get_gamebtns(uint32_t* btn) {
    assert(ekt_io_backend != NULL);
    assert(btn != NULL);

    ekt_io_backend->get_gamebtns(btn);
}

void ekt_io_get_trackball_position(uint16_t* stick_x, uint16_t* stick_y) {
    assert(ekt_io_backend != NULL);
    assert(stick_x != NULL);
    assert(stick_y != NULL);

    ekt_io_backend->get_trackball(stick_x, stick_y);
}

HRESULT ekt_io_led_init(void) {
    return S_OK;
}

void ekt_io_led_set_colors(uint8_t board, uint8_t* rgb) {
    return;
}
