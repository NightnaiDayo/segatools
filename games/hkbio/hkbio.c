#include <windows.h>

#include <stdint.h>

#include "hkbio/hkbio.h"

#include <assert.h>

#include "hkbio/keyboard.h"
#include "hkbio/di.h"
#include "hkbio/config.h"
#include "util/dprintf.h"
#include "util/env.h"
#include "util/str.h"

static uint8_t hkb_opbtn;
static struct hkb_io_config hkb_io_cfg;
static const struct hkb_io_backend* hkb_io_backend;
static bool hkb_io_coin;

uint16_t hkb_io_get_api_version(void) {
    return 0x0100;
}

HRESULT hkb_io_init(void) {
    hkb_io_config_load(&hkb_io_cfg, get_config_path());

    HRESULT hr;

    HINSTANCE inst = GetModuleHandleW(NULL);

    if (inst == NULL) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        dprintf("GetModuleHandleW failed: %lx\n", hr);

        return hr;
    }

    if (wstr_ieq(hkb_io_cfg.mode, L"keyboard")) {
        hr = hkb_kb_init(&hkb_io_cfg.kb, &hkb_io_backend);
    } else if (wstr_ieq(hkb_io_cfg.mode, L"dinput")) {
        hr = hkb_di_init(&hkb_io_cfg.di, inst, &hkb_io_backend);
    } else {
        hr = E_INVALIDARG;
        dprintf("HKB IO: Invalid IO mode \"%S\", use keyboard or dinput\n", hkb_io_cfg.mode);
    }

    return hr;
}

HRESULT hkb_io_poll(void) {
    assert(hkb_io_backend != NULL);

    hkb_opbtn = 0;

    if (GetAsyncKeyState(hkb_io_cfg.vk_test) & 0x8000) {
        hkb_opbtn |= HKB_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(hkb_io_cfg.vk_service) & 0x8000) {
        hkb_opbtn |= HKB_IO_OPBTN_SERVICE;
    }

    if (GetAsyncKeyState(hkb_io_cfg.vk_coin) & 0x8000) {
        if (!hkb_io_coin) {
            hkb_io_coin = true;
            hkb_opbtn |= HKB_IO_OPBTN_COIN;
        }
    } else {
        hkb_io_coin = false;
    }

    return S_OK;
}

void hkb_io_get_opbtns(uint8_t* opbtn) {
    assert(hkb_io_backend != NULL);
    assert(opbtn != NULL);

    if (hkb_io_backend->get_opbtns != NULL) {
        hkb_io_backend->get_opbtns(opbtn);
    }

    *opbtn |= hkb_opbtn;
}

void hkb_io_get_gamebtns(uint16_t* btn) {
    assert(hkb_io_backend != NULL);
    assert(btn != NULL);

    if (hkb_io_backend->get_gamebtns != NULL) {
        hkb_io_backend->get_gamebtns(btn);
    }
}

HRESULT hkb_io_led_init(void) {
    return S_OK;
}

void hkb_io_led_set_colors(uint8_t board, uint8_t* rgb) {
    return;
}
