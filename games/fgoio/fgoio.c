#include <windows.h>
#include <xinput.h>
#include <math.h>

#include <stdint.h>

#include "fgoio/fgoio.h"

#include <assert.h>

#include "keyboard.h"
#include "xi.h"
#include "fgoio/config.h"
#include "util/dprintf.h"
#include "util/env.h"
#include "util/str.h"

static uint8_t fgo_opbtn;
static uint8_t fgo_gamebtn;
static int16_t fgo_stick_x;
static int16_t fgo_stick_y;
static struct fgo_io_config fgo_io_cfg;
static const struct fgo_io_backend* fgo_io_backend;
static bool fgo_io_coin;

uint16_t fgo_io_get_api_version(void) {
    return 0x0100;
}

HRESULT fgo_io_init(void) {
    fgo_io_config_load(&fgo_io_cfg, get_config_path());

    HRESULT hr;

    if (wstr_ieq(fgo_io_cfg.mode, L"keyboard")) {
        hr = fgo_kb_init(&fgo_io_cfg.kb, &fgo_io_backend);
    } else if (wstr_ieq(fgo_io_cfg.mode, L"xinput")) {
        hr = fgo_xi_init(&fgo_io_cfg.xi, &fgo_io_backend);
    } else {
        hr = E_INVALIDARG;
        dprintf("FGO IO: Invalid IO mode \"%S\", use keyboard or xinput\n",
                fgo_io_cfg.mode);
    }

    return hr;
}

HRESULT fgo_io_poll(void) {
    assert(fgo_io_backend != NULL);

    fgo_opbtn = 0;
    fgo_gamebtn = 0;
    fgo_stick_x = 0;
    fgo_stick_y = 0;

    if (GetAsyncKeyState(fgo_io_cfg.vk_test) & 0x8000) {
        fgo_opbtn |= FGO_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(fgo_io_cfg.vk_service) & 0x8000) {
        fgo_opbtn |= FGO_IO_OPBTN_SERVICE;
    }

    if (GetAsyncKeyState(fgo_io_cfg.vk_coin) & 0x8000) {
        if (!fgo_io_coin) {
            fgo_io_coin = true;
            fgo_opbtn |= FGO_IO_OPBTN_COIN;
        }
    } else {
        fgo_io_coin = false;
    }

    return S_OK;
}

void fgo_io_get_opbtns(uint8_t* opbtn) {
    if (opbtn != NULL) {
        *opbtn = fgo_opbtn;
    }
}

void fgo_io_get_gamebtns(uint8_t* btn) {
    assert(fgo_io_backend != NULL);
    assert(btn != NULL);

    fgo_io_backend->get_gamebtns(btn);
}

void fgo_io_get_analogs(int16_t* stick_x, int16_t* stick_y) {
    assert(fgo_io_backend != NULL);
    assert(stick_x != NULL);
    assert(stick_y != NULL);

    fgo_io_backend->get_analogs(stick_x, stick_y);
}

HRESULT fgo_io_led_init(void) {
    return S_OK;
}

void fgo_io_led_set_colors(uint8_t board, uint8_t* rgb) {
    return;
}
