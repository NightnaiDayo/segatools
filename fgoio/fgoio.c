#include <windows.h>
#include <xinput.h>
#include <math.h>

#include <limits.h>
#include <stdint.h>

#include "fgoio/fgoio.h"
#include "fgoio/config.h"
#include "util/dprintf.h"

static uint8_t fgo_opbtn;
static uint8_t fgo_gamebtn;
static int16_t fgo_stick_x;
static int16_t fgo_stick_y;
static struct fgo_io_config fgo_io_cfg;
static bool fgo_io_coin;

uint16_t fgo_io_get_api_version(void)
{
    return 0x0100;
}

HRESULT fgo_io_init(void)
{
    fgo_io_config_load(&fgo_io_cfg, L".\\segatools.ini");

    return S_OK;
}

HRESULT fgo_io_poll(void)
{
    XINPUT_STATE xi;
    WORD xb;

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

    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);
    xb = xi.Gamepad.wButtons;

    if (xi.Gamepad.bLeftTrigger > 64) {
        fgo_gamebtn |= FGO_IO_GAMEBTN_SPEED_UP;
    }

    if (xb & XINPUT_GAMEPAD_LEFT_SHOULDER) {
        fgo_gamebtn |= FGO_IO_GAMEBTN_TARGET;
    }

    if (xb & XINPUT_GAMEPAD_A || xb & XINPUT_GAMEPAD_B) {
        fgo_gamebtn |= FGO_IO_GAMEBTN_ATTACK;
    }

    if (xb & XINPUT_GAMEPAD_Y || xb & XINPUT_GAMEPAD_X) {
        fgo_gamebtn |= FGO_IO_GAMEBTN_NOBLE_PHANTASHM;
    }
    
    if (xb & XINPUT_GAMEPAD_LEFT_THUMB) {
        fgo_gamebtn |= FGO_IO_GAMEBTN_CAMERA;
    }

    float LX = xi.Gamepad.sThumbLX;
    float LY = xi.Gamepad.sThumbLY;

    // determine how far the controller is pushed
    float magnitude = sqrt(LX*LX + LY*LY);

    // determine the direction the controller is pushed
    float normalizedLX = LX / magnitude;
    float normalizedLY = LY / magnitude;

    float normalizedMagnitude = 0;

    // check if the controller is outside a circular dead zone
    if (magnitude > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
    {
        // clip the magnitude at its expected maximum value
        if (magnitude > 32767) magnitude = 32767;

        // adjust magnitude relative to the end of the dead zone
        magnitude -= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;

        // optionally normalize the magnitude with respect to its expected range
        // giving a magnitude value of 0.0 to 1.0
        normalizedMagnitude = magnitude / (32767 - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    } else // if the controller is in the deadzone zero out the magnitude
    {
        magnitude = 0.0;
        normalizedMagnitude = 0.0;
    }

    fgo_stick_x = (int16_t)(normalizedLX * normalizedMagnitude * 32767);
    fgo_stick_y = (int16_t)(normalizedLY * normalizedMagnitude * 32767);

    return S_OK;
}

void fgo_io_get_opbtns(uint8_t *opbtn)
{
    if (opbtn != NULL) {
        *opbtn = fgo_opbtn;
    }
}

void fgo_io_get_gamebtns(uint8_t *btn)
{
    if (btn != NULL) {
        *btn = fgo_gamebtn;
    }
}

void fgo_io_get_analogs(int16_t *stick_x, int16_t *stick_y)
{
    if (stick_x != NULL) {
        *stick_x = fgo_stick_x;
    }

    if (stick_y != NULL) {
        *stick_y = fgo_stick_y;
    }
}

HRESULT fgo_io_led_init(void)
{
    return S_OK;
}

void fgo_io_led_set_colors(uint8_t board, uint8_t *rgb)
{
    return;
}
