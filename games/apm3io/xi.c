#include <windows.h>
#include <xinput.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "apm3io/backend.h"
#include "apm3io/config.h"
#include "apm3io/apm3io.h"
#include "apm3io/xi.h"

#include "util/dprintf.h"

static void apm3_xi_get_gamebtns(uint16_t *gamebtn_out);

static HRESULT apm3_xi_config_apply(const struct apm3_xi_config *cfg);

static const struct apm3_io_backend apm3_xi_backend = {
    .get_gamebtns  = apm3_xi_get_gamebtns,
};

static bool apm3_xi_analog_stick_enabled;

HRESULT apm3_xi_init(const struct apm3_xi_config *cfg, const struct apm3_io_backend **backend)
{
    HRESULT hr;
    assert(cfg != NULL);
    assert(backend != NULL);

    hr = apm3_xi_config_apply(cfg);

    if (FAILED(hr)) {
        return hr;
    }

    dprintf("XInput: Using XInput controller\n");
    *backend = &apm3_xi_backend;

    return S_OK;
}

HRESULT apm3_io_poll(void)
{    
    return S_OK;
}

static HRESULT apm3_xi_config_apply(const struct apm3_xi_config *cfg)
{
    dprintf("XInput: --- Begin configuration ---\n");
    dprintf("XInput: Analog Stick : %i\n", cfg->analog_stick_enabled);
    dprintf("XInput: ---  End  configuration ---\n");

    apm3_xi_analog_stick_enabled = cfg->analog_stick_enabled;

    return S_OK;
}

static void apm3_xi_get_gamebtns(uint16_t *gamebtn_out)
{
    uint16_t gamebtn;
    XINPUT_STATE xi;
    WORD xb;
    int left_x, left_y;

    assert(gamebtn_out != NULL);

    gamebtn = 0;

    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);
    xb = xi.Gamepad.wButtons;

    /* Use the left analog stick as a D Pad if enabled */
    if (apm3_xi_analog_stick_enabled) {
        left_x = xi.Gamepad.sThumbLX;
        left_y = xi.Gamepad.sThumbLY;

        if (left_x < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2) {
            left_x += XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2;
        } else if (left_x > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2) {
            left_x -= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2;
        } else {
            left_x = 0;
        }

        if (left_y < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2) {
            left_y += XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2;
        } else if (left_y > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2) {
            left_y -= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2;
        } else {
            left_y = 0;
        }

        if (left_y < 0) {
            gamebtn |= APM3_IO_GAMEBTN_DOWN;
        } else if (left_y > 0) {
            gamebtn |= APM3_IO_GAMEBTN_UP;
        }    

        if (left_x < 0) {
            gamebtn |= APM3_IO_GAMEBTN_LEFT;
        } else if (left_x > 0) {
            gamebtn |= APM3_IO_GAMEBTN_RIGHT;
        }
    }

    /* Normal game button controls */

    if (xb & XINPUT_GAMEPAD_DPAD_UP) {
        gamebtn |= APM3_IO_GAMEBTN_UP;
    }

    if (xb & XINPUT_GAMEPAD_DPAD_DOWN) {
        gamebtn |= APM3_IO_GAMEBTN_DOWN;
    }

    if (xb & XINPUT_GAMEPAD_DPAD_LEFT) {
        gamebtn |= APM3_IO_GAMEBTN_LEFT;
    }

    if (xb & XINPUT_GAMEPAD_DPAD_RIGHT) {
        gamebtn |= APM3_IO_GAMEBTN_RIGHT;
    }

    if (xb & XINPUT_GAMEPAD_START) {
        gamebtn |= APM3_IO_GAMEBTN_START;
    }

    if (xb & XINPUT_GAMEPAD_BACK) {
        gamebtn |= APM3_IO_GAMEBTN_HOME;
    }

    if (xb & XINPUT_GAMEPAD_A) {
        gamebtn |= APM3_IO_GAMEBTN_B1;
    }

    if (xb & XINPUT_GAMEPAD_B) {
        gamebtn |= APM3_IO_GAMEBTN_B2;
    }

    if (xb & XINPUT_GAMEPAD_X) {
        gamebtn |= APM3_IO_GAMEBTN_B5;
    }

    if (xb & XINPUT_GAMEPAD_Y) {
        gamebtn |= APM3_IO_GAMEBTN_B6;
    }

    if (xb & XINPUT_GAMEPAD_LEFT_SHOULDER) {
        gamebtn |= APM3_IO_GAMEBTN_B3;
    }

    if (xb & XINPUT_GAMEPAD_RIGHT_SHOULDER) {
        gamebtn |= APM3_IO_GAMEBTN_B4;
    }

    if (xi.Gamepad.bLeftTrigger > 64) {
        gamebtn |= APM3_IO_GAMEBTN_B7;
    }

    if (xi.Gamepad.bRightTrigger > 64) {
        gamebtn |= APM3_IO_GAMEBTN_B8;
    }

    *gamebtn_out = gamebtn;
}
