#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "tokyoio/backend.h"
#include "tokyoio/config.h"
#include "tokyoio/kb.h"
#include "tokyoio/tokyoio.h"
#include "tokyoio/xi.h"

#include "util/dprintf.h"
#include "util/str.h"
#include "util/env.h"

static struct tokyo_io_config tokyo_io_cfg;
static const struct tokyo_io_backend *tokyo_io_backend;
static bool tokyo_io_coin;

uint16_t tokyo_io_get_api_version(void)
{
    return 0x0100;
}

HRESULT tokyo_io_init(void)
{
    HINSTANCE inst;
    HRESULT hr;

    assert(tokyo_io_backend == NULL);

    inst = GetModuleHandleW(NULL);

    if (inst == NULL) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        dprintf("GetModuleHandleW failed: %lx\n", hr);

        return hr;
    }

    tokyo_io_config_load(&tokyo_io_cfg, get_config_path());

    if (wstr_ieq(tokyo_io_cfg.mode, L"keyboard")) {
        hr = tokyo_kb_init(&tokyo_io_cfg.kb, &tokyo_io_backend);
    } else if (wstr_ieq(tokyo_io_cfg.mode, L"xinput")) {
        hr = tokyo_xi_init(&tokyo_io_backend);
    } else {
        hr = E_INVALIDARG;
        dprintf("IDAC IO: Invalid IO mode \"%S\", use keyboard or xinput\n",
                tokyo_io_cfg.mode);
    }

    return hr;
}

void tokyo_io_get_opbtns(uint8_t *opbtn_out)
{
    uint8_t opbtn;

    assert(tokyo_io_backend != NULL);
    assert(opbtn_out != NULL);

    opbtn = 0;

    /* Common operator buttons, not backend-specific */

    if (GetAsyncKeyState(tokyo_io_cfg.vk_test) & 0x8000) {
        opbtn |= TOKYO_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(tokyo_io_cfg.vk_service) & 0x8000) {
        opbtn |= TOKYO_IO_OPBTN_SERVICE;
    }

    if (GetAsyncKeyState(tokyo_io_cfg.vk_coin) & 0x8000) {
        if (!tokyo_io_coin) {
            tokyo_io_coin = true;
            opbtn |= TOKYO_IO_OPBTN_COIN;
        }
    } else {
        tokyo_io_coin = false;
    }

    *opbtn_out = opbtn;
}


void tokyo_io_get_gamebtns(uint8_t *gamebtn_out)
{
    assert(tokyo_io_backend != NULL);
    assert(gamebtn_out != NULL);

    tokyo_io_backend->get_gamebtns(gamebtn_out);
}

void tokyo_io_get_sensors(uint8_t *sense_out)
{
    assert(sense_out != NULL);
    assert(tokyo_io_backend != NULL);

    tokyo_io_backend->get_sensors(sense_out);
}

HRESULT tokyo_io_led_init(void)
{
    return S_OK;
}

void tokyo_io_led_set_colors(uint8_t board, uint8_t *rgb)
{
#if 0
    if (board == 0) {
        dprintf("Board 0:\n");
        // Change GRB order to RGB order
        for (int i = 0; i < 27; i++) {
            dprintf("Tokyo LED: MONITOR LEFT: %02X %02X %02X\n", rgb[i * 3 + 1], rgb[i * 3], rgb[i * 3 + 2]);
        }

        for (int i = 27; i < 54; i++) {
            dprintf("Tokyo LED: MONITOR RIGHT: %d, %02X %02X %02X\n", i, rgb[i * 3 + 1], rgb[i * 3], rgb[i * 3 + 2]);
        }
    } else {
        dprintf("Board 1:\n");
        dprintf("Tokyo LED: LEFT BLUE: %02X\n", rgb[0]);
        dprintf("Tokyo LED: CENTER YELLOW: %02X\n", rgb[1]);
        dprintf("Tokyo LED: RIGHT RED: %02X\n", rgb[2]);
        dprintf("Tokyo LED: CONTROL LEFT: %02X %02X %02X\n", rgb[3], rgb[4], rgb[5]);
        dprintf("Tokyo LED: CONTROL RIGHT: %02X %02X %02X\n", rgb[6], rgb[7], rgb[8]);
        dprintf("Tokyo LED: FLOOR LEFT: %02X %02X %02X\n", rgb[9], rgb[10], rgb[11]);
        dprintf("Tokyo LED: FLOOR RIGHT: %02X %02X %02X\n", rgb[12], rgb[13], rgb[14]);
    }
#endif

    return;
}
