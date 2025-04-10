#include <windows.h>
#include <xinput.h>

#include <math.h>
#include <assert.h>
#include <stdint.h>

#include "fgoio/backend.h"
#include "fgoio/config.h"
#include "fgoio/fgoio.h"
#include "fgoio/xi.h"

#include "util/dprintf.h"

static void fgo_xi_get_gamebtns(uint8_t* gamebtn_out);
static void fgo_xi_get_analogs(int16_t* x, int16_t* y);
static HRESULT fgo_xi_config_apply(const struct fgo_xi_config* cfg);

static const struct fgo_io_backend fgo_xi_backend = {
    .get_gamebtns = fgo_xi_get_gamebtns,
    .get_analogs = fgo_xi_get_analogs
};

static float fgo_xi_stick_deadzone;

const uint16_t max_stick_value = 32767;

HRESULT fgo_xi_init(const struct fgo_xi_config* cfg, const struct fgo_io_backend** backend) {
    assert(cfg != NULL);
    assert(backend != NULL);

    HRESULT hr = fgo_xi_config_apply(cfg);

    if (FAILED(hr)) {
        return hr;
    }

    dprintf("XInput: Using XInput controller\n");
    *backend = &fgo_xi_backend;

    return S_OK;
}

static HRESULT fgo_xi_config_apply(const struct fgo_xi_config* cfg) {
    /* Deadzone check */
    if (cfg->stick_deadzone > 32767 || cfg->stick_deadzone < 0) {
        dprintf("XInput: Stick deadzone is too large or negative\n");
        return E_INVALIDARG;
    }

    dprintf("XInput: --- Begin configuration ---\n");
    dprintf("XInput: Left Deadzone . . . . : %i\n", cfg->stick_deadzone);
    dprintf("XInput: ---  End  configuration ---\n");

    fgo_xi_stick_deadzone = cfg->stick_deadzone;

    return S_OK;
}

static void fgo_xi_get_gamebtns(uint8_t* gamebtn_out) {
    assert(gamebtn_out != NULL);

    XINPUT_STATE xi;
    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);

    uint8_t gamebtn = 0;
    WORD xb = xi.Gamepad.wButtons;

    if (xi.Gamepad.bLeftTrigger > 64) {
        gamebtn |= FGO_IO_GAMEBTN_SPEED_UP;
    }

    if (xb & XINPUT_GAMEPAD_LEFT_SHOULDER) {
        gamebtn |= FGO_IO_GAMEBTN_TARGET;
    }

    if (xb & XINPUT_GAMEPAD_A || xb & XINPUT_GAMEPAD_B) {
        gamebtn |= FGO_IO_GAMEBTN_ATTACK;
    }

    if (xb & XINPUT_GAMEPAD_Y || xb & XINPUT_GAMEPAD_X) {
        gamebtn |= FGO_IO_GAMEBTN_NOBLE_PHANTASM;
    }

    if (xb & XINPUT_GAMEPAD_LEFT_THUMB) {
        gamebtn |= FGO_IO_GAMEBTN_CAMERA;
    }

    *gamebtn_out = gamebtn;
}

static void fgo_xi_get_analogs(int16_t* x, int16_t* y) {

    assert(x != NULL);
    assert(y != NULL);

    XINPUT_STATE xi;
    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);

    float LX = xi.Gamepad.sThumbLX;
    float LY = xi.Gamepad.sThumbLY;

    // determine how far the controller is pushed
    float magnitude = sqrt(LX * LX + LY * LY);

    // determine the direction the controller is pushed
    float normalizedLX = LX / magnitude;
    float normalizedLY = LY / magnitude;

    float normalizedMagnitude = 0;

    // check if the controller is outside a circular dead zone
    if (magnitude > fgo_xi_stick_deadzone) {
        // clip the magnitude at its expected maximum value
        if (magnitude > 32767) magnitude = 32767;

        // adjust magnitude relative to the end of the dead zone
        magnitude -= fgo_xi_stick_deadzone;

        // optionally normalize the magnitude with respect to its expected range
        // giving a magnitude value of 0.0 to 1.0
        normalizedMagnitude = magnitude / (32767 - fgo_xi_stick_deadzone);
    } else // if the controller is in the deadzone zero out the magnitude
    {
        normalizedMagnitude = 0;
    }

    *x = (int16_t) (normalizedLX * normalizedMagnitude * 32767);
    *y = (int16_t) (normalizedLY * normalizedMagnitude * 32767);
}
