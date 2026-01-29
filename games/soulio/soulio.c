#include <windows.h>
#include <xinput.h>

#include <limits.h>
#include <stdint.h>

#include "soulio/soulio.h"
#include "soulio/config.h"

#include "util/dprintf.h"
#include "util/env.h"

static uint8_t soul_opbtn;
static uint8_t soul_gamebtn;
static struct soul_io_config soul_io_cfg;
static bool soul_io_coin;

uint16_t soul_io_get_api_version(void)
{
    return 0x0100;
}

HRESULT soul_io_init(void)
{
    soul_io_config_load(&soul_io_cfg, get_config_path());
    return S_OK;
}

HRESULT soul_io_poll(void)
{
    int lever;
    int xlever;
    XINPUT_STATE xi;
    WORD xb;

    soul_opbtn = 0;
    soul_gamebtn = 0;

    if (GetAsyncKeyState(soul_io_cfg.vk_test) & 0x8000) {
        soul_opbtn |= SOUL_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(soul_io_cfg.vk_service) & 0x8000) {
        soul_opbtn |= SOUL_IO_OPBTN_SERVICE;
    }

    if (GetAsyncKeyState(soul_io_cfg.vk_coin) & 0x8000) {
        if (!soul_io_coin) {
            soul_io_coin = true;
            soul_opbtn |= SOUL_IO_OPBTN_COIN;
        }
    } else {
        soul_io_coin = false;
    }

    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);
    xb = xi.Gamepad.wButtons;

    if (xb & XINPUT_GAMEPAD_X) {
        soul_gamebtn |= SOUL_IO_GAMEBTN_A;
    }

    if (xb & XINPUT_GAMEPAD_A) {
        soul_gamebtn |= SOUL_IO_GAMEBTN_B;
    }

    if (xb & XINPUT_GAMEPAD_B) {
        soul_gamebtn |= SOUL_IO_GAMEBTN_C;
    }

    return S_OK;
}

void soul_io_get_opbtns(uint8_t *opbtn)
{
    if (opbtn != NULL) {
        *opbtn = soul_opbtn;
    }
}

void soul_io_get_gamebtns(uint8_t *gamebtn)
{
    if (gamebtn != NULL) {
        *gamebtn = soul_gamebtn;
    }
}

HRESULT soul_io_led_init(void)
{
    return S_OK;
}

void soul_io_led_set_colors(uint8_t board, uint8_t *rgb)
{
    return;
}
