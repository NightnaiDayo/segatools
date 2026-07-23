#include <windows.h>
#include <xinput.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "divaio/config.h"
#include "divaio/input/backend.h"
#include "divaio/input/xi.h"

#include "util/dprintf.h"

static void diva_xi_get_gamebtns(struct diva_button_states* states);
static void diva_xi_get_opbtns(uint8_t* opbtn);
static void diva_xi_get_auto_status(bool* left, bool* right);

static HRESULT diva_xi_config_apply(const struct diva_xi_config* cfg);


static const struct diva_io_backend diva_xi_backend = {
    .get_gamebtns    = diva_xi_get_gamebtns,
    .get_opbtns      = diva_xi_get_opbtns,
    .get_auto_status = diva_xi_get_auto_status,
};

static bool diva_xi_test_enabled;

static XINPUT_STATE xi = {0};

HRESULT diva_xi_init(const struct diva_xi_config* cfg, const struct diva_io_backend** backend) {
    assert(cfg != NULL);
    assert(backend != NULL);

    HRESULT hr = diva_xi_config_apply(cfg);

    if (FAILED(hr)) {
        return hr;
    }

    dprintf("XInput: Using XInput controller\n");
    *backend = &diva_xi_backend;

    return S_OK;
}

static HRESULT diva_xi_config_apply(const struct diva_xi_config* cfg) {
    dprintf("XInput: --- Begin configuration ---\n");
    dprintf("XInput: Test Button : %i\n", cfg->use_select_as_test);
    dprintf("XInput: ---  End  configuration ---\n");

    diva_xi_test_enabled = cfg->use_select_as_test;

    return S_OK;
}

static void diva_xi_get_gamebtns(struct diva_button_states* states) {

    assert(states != NULL);

    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);

    const WORD xb = xi.Gamepad.wButtons;

    states->start = xb & XINPUT_GAMEPAD_START;
    states->cross.primary = xb & XINPUT_GAMEPAD_A;
    states->cross.secondary = xb & XINPUT_GAMEPAD_DPAD_DOWN;
    states->circle.primary = xb & XINPUT_GAMEPAD_B;
    states->circle.secondary = xb & XINPUT_GAMEPAD_DPAD_RIGHT;
    states->triangle.primary = xb & XINPUT_GAMEPAD_Y;
    states->triangle.secondary = xb & XINPUT_GAMEPAD_DPAD_UP;
    states->square.primary = xb & XINPUT_GAMEPAD_X;
    states->square.secondary = xb & XINPUT_GAMEPAD_DPAD_LEFT;
}

void diva_xi_get_opbtns(uint8_t* opbtn) {
    assert(opbtn != NULL);

    uint8_t opbtn_out = 0;

    if (diva_xi_test_enabled && xi.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) {
        opbtn_out |= DIVA_IO_OPBTN_TEST;
    }

    *opbtn = opbtn_out;
}

void diva_xi_get_auto_status(bool* left, bool* right) {
    assert(left != NULL);
    assert(right != NULL);

    *left = false;
    *right = false;

    const int left_x = xi.Gamepad.sThumbLX;
    const int right_x = xi.Gamepad.sThumbRX;

    if (left_x < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2) {
        *left |= true;
    } else if (left_x > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2) {
        *right |= true;
    }

    if (right_x < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2) {
        *left |= true;
    } else if (right_x > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE * 2) {
        *right |= true;
    }

    const WORD xb = xi.Gamepad.wButtons;

    if (xb & XINPUT_GAMEPAD_LEFT_SHOULDER) {
        *left |= true;
    }

    if (xb & XINPUT_GAMEPAD_RIGHT_SHOULDER) {
        *right |= true;
    }

    if (xi.Gamepad.bLeftTrigger > 64) {
        *left |= true;
    }

    if (xi.Gamepad.bRightTrigger > 64) {
        *right |= true;
    }
}
