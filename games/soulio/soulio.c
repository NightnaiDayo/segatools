#include "soulio/soulio.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <windows.h>
#include <xinput.h>

#include "soulio/config.h"
#include "util/dprintf.h"
#include "util/env.h"

static uint8_t soul_opbtn;
static uint8_t soul_gamebtn;
static struct soul_io_config soul_io_cfg;
static bool soul_io_coin;

static uint16_t soul_xi_stick_deadzone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
static uint16_t soul_xi_view_deadzone = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;

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
    soul_opbtn = 0;

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

    return S_OK;
}

void soul_io_get_opbtns(uint8_t* opbtn)
{
    if (opbtn != NULL) {
        *opbtn = soul_opbtn;
    }
}

void soul_io_get_gamebtns(uint8_t* gamebtn_out)
{
    XINPUT_STATE xi;
    WORD xb;

    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);
    xb = xi.Gamepad.wButtons;

    uint8_t gamebtn = 0;

    if (xb & XINPUT_GAMEPAD_LEFT_THUMB) {
        gamebtn |= SOUL_IO_STICK_PUSH;
    }

    if (xb & XINPUT_GAMEPAD_LEFT_SHOULDER) {
        gamebtn |= SOUL_IO_GAMEBTN_L1;
    }

    if (xi.Gamepad.bLeftTrigger > 64) {
        gamebtn |= SOUL_IO_GAMEBTN_L2;
    }

    if (xb & XINPUT_GAMEPAD_RIGHT_THUMB) {
        gamebtn |= SOUL_IO_VIEW_STICK_PUSH;
    }

    if (xb & XINPUT_GAMEPAD_X) {
        gamebtn |= SOUL_IO_GAMEBTN_A;
    }

    if (xb & XINPUT_GAMEPAD_A) {
        gamebtn |= SOUL_IO_GAMEBTN_B;
    }

    if (xb & XINPUT_GAMEPAD_Y) {
        gamebtn |= SOUL_IO_GAMEBTN_B;
    }

    if (xb & XINPUT_GAMEPAD_B) {
        gamebtn |= SOUL_IO_GAMEBTN_C;
    }

    *gamebtn_out = gamebtn;
}

static void process_stick(
    int16_t raw_x,
    int16_t raw_y,
    int16_t* out_x,
    int16_t* out_y)
{
    float LX = (float)raw_x;
    float LY = (float)raw_y;

    float magnitude = sqrtf(LX * LX + LY * LY);

    if (magnitude > soul_xi_stick_deadzone) {
        float normLX = LX / magnitude;
        float normLY = LY / magnitude;

        if (magnitude > 32767.0f) magnitude = 32767.0f;

        float normalizedMag = (magnitude - soul_xi_stick_deadzone) / (32767.0f - soul_xi_stick_deadzone);

        *out_x = (int16_t)(normLX * normalizedMag * 32767.0f);
        *out_y = (int16_t)(normLY * normalizedMag * 32767.0f);
    } else {
        *out_x = 0;
        *out_y = 0;
    }
}

void soul_io_get_analogs(
    int16_t* stick_x,
    int16_t* stick_y,
    int16_t* view_x,        
    int16_t* view_y)
{
    assert(stick_x != NULL);
    assert(stick_y != NULL);
    assert(view_x != NULL);
    assert(view_y != NULL);

    XINPUT_STATE xi;
    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);

    // Left stick
    process_stick(xi.Gamepad.sThumbLX, xi.Gamepad.sThumbLY, stick_x, stick_y);

    // Right (view) stick
    process_stick(xi.Gamepad.sThumbRX, xi.Gamepad.sThumbRY, view_x, view_y);
}

HRESULT soul_io_led_init(void)
{
    return S_OK;
}

void soul_io_led_set_colors(uint8_t board, uint8_t* rgb)
{
    return;
}
