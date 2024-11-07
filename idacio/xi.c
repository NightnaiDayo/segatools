#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include <xinput.h>

#include "idacio/xi.h"
#include "idacio/backend.h"
#include "idacio/config.h"
#include "idacio/idacio.h"
#include "idacio/shifter.h"

#include "util/dprintf.h"

static void idac_xi_get_gamebtns(uint8_t *gamebtn_out);
static void idac_xi_get_shifter(uint8_t *gear);
static void idac_xi_get_analogs(struct idac_io_analog_state *out);

static HRESULT idac_xi_ffb_init(void);
static void idac_xi_ffb_toggle(bool active);
static void idac_xi_ffb_constant_force(uint8_t direction, uint8_t force);
static void idac_xi_ffb_rumble(uint8_t force, uint8_t period);
static void idac_xi_ffb_damper(uint8_t force);

static HRESULT idac_xi_config_apply(const struct idac_xi_config *cfg);

static const struct idac_io_backend idac_xi_backend = {
    .get_gamebtns       = idac_xi_get_gamebtns,
    .get_shifter        = idac_xi_get_shifter,
    .get_analogs        = idac_xi_get_analogs,
    .ffb_init           = idac_xi_ffb_init,
    .ffb_toggle         = idac_xi_ffb_toggle,
    .ffb_constant_force = idac_xi_ffb_constant_force,
    .ffb_rumble         = idac_xi_ffb_rumble,
    .ffb_damper         = idac_xi_ffb_damper
};

static bool idac_xi_single_stick_steering;
static bool idac_xi_linear_steering;
static uint16_t idac_xi_left_stick_deadzone;
static uint16_t idac_xi_right_stick_deadzone;

const uint16_t max_stick_value = 32767;
/* Apply steering wheel restriction. Real cabs only report about 76% of
    the output value when the wheel is turned to either of its maximum positions. */
const uint16_t max_wheel_value = 24831;

HRESULT idac_xi_init(const struct idac_xi_config *cfg, const struct idac_io_backend **backend) {
    HRESULT hr;
    assert(cfg != NULL);
    assert(backend != NULL);

    hr = idac_xi_config_apply(cfg);

    if (FAILED(hr)) {
        return hr;
    }

    dprintf("IDACIO: Using XInput controller\n");
    *backend = &idac_xi_backend;

    return S_OK;
}

static HRESULT idac_xi_config_apply(const struct idac_xi_config *cfg) {
    /* Deadzones check */
    if (cfg->left_stick_deadzone > 32767 || cfg->left_stick_deadzone < 0) {
        dprintf("XInput: Left stick deadzone is too large or negative\n");
        return E_INVALIDARG;
    }

    if (cfg->right_stick_deadzone > 32767 || cfg->right_stick_deadzone < 0) {
        dprintf("XInput: Right stick deadzone is too large or negative\n");
        return E_INVALIDARG;
    }

    dprintf("XInput: --- Begin configuration ---\n");
    dprintf("XInput: Single Stick Steering : %i\n", cfg->single_stick_steering);
    dprintf("XInput: Linear Steering . . . : %i\n", cfg->linear_steering);
    dprintf("XInput: Left Deadzone . . . . : %i\n", cfg->left_stick_deadzone);
    dprintf("XInput: Right Deadzone  . . . : %i\n", cfg->right_stick_deadzone);
    dprintf("XInput: ---  End  configuration ---\n");

    idac_xi_single_stick_steering = cfg->single_stick_steering;
    idac_xi_linear_steering = cfg->linear_steering;
    idac_xi_left_stick_deadzone = cfg->left_stick_deadzone;
    idac_xi_right_stick_deadzone = cfg->right_stick_deadzone;

    return S_OK;
}

static void idac_xi_get_gamebtns(uint8_t *gamebtn_out) {
    uint8_t gamebtn;
    XINPUT_STATE xi;
    WORD xb;

    assert(gamebtn_out != NULL);

    gamebtn = 0;

    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);
    xb = xi.Gamepad.wButtons;

    if (xb & XINPUT_GAMEPAD_DPAD_UP) {
        gamebtn |= IDAC_IO_GAMEBTN_UP;
    }

    if (xb & XINPUT_GAMEPAD_DPAD_DOWN) {
        gamebtn |= IDAC_IO_GAMEBTN_DOWN;
    }

    if (xb & (XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_LEFT_THUMB)) {
        gamebtn |= IDAC_IO_GAMEBTN_LEFT;
    }

    if (xb & (XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_RIGHT_THUMB)) {
        gamebtn |= IDAC_IO_GAMEBTN_RIGHT;
    }

    if (xb & (XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_A)) {
        gamebtn |= IDAC_IO_GAMEBTN_START;
    }

    if (xb & (XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_B)) {
        gamebtn |= IDAC_IO_GAMEBTN_VIEW_CHANGE;
    }

    *gamebtn_out = gamebtn;
}

static void idac_xi_get_shifter(uint8_t *gear) {
    bool shift_dn;
    bool shift_up;
    XINPUT_STATE xi;
    WORD xb;

    assert(gear != NULL);

    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);
    xb = xi.Gamepad.wButtons;

    if (xb & XINPUT_GAMEPAD_START) {
        /* Reset to Neutral when start is pressed */
        idac_shifter_set(0);
    }

    shift_dn = xb & (XINPUT_GAMEPAD_Y | XINPUT_GAMEPAD_LEFT_SHOULDER);
    shift_up = xb & (XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_RIGHT_SHOULDER);

    idac_shifter_update(shift_dn, shift_up);

    *gear = idac_shifter_current_gear();
}

static int16_t calculate_norm_steering(int16_t axis, uint16_t deadzone, bool linear_steering) {
    // determine how far the controller is pushed
    float magnitude = sqrt(axis*axis);

    // determine the direction the controller is pushed
    float norm_axis = axis / magnitude;

    float norm_magnitude = 0.0;

    // check if the controller is outside a circular dead zone
    if (magnitude > deadzone)
    {
        // clip the magnitude at its expected maximum value
        if (magnitude > max_stick_value) magnitude = max_stick_value;

        // adjust magnitude relative to the end of the dead zone
        magnitude -= deadzone;

        // optionally normalize the magnitude with respect to its expected range
        // giving a magnitude value of 0.0 to 1.0
        norm_magnitude = magnitude / (max_stick_value - deadzone);
    } else // if the controller is in the deadzone zero out the magnitude
    {
        magnitude = 0.0;
        norm_magnitude = 0.0;
    }

    // apply non-linear transform to the axis
    if (!linear_steering) {
        return norm_axis * pow(norm_magnitude, 3.0) * max_wheel_value;
    }

    return norm_axis * norm_magnitude * max_wheel_value;
}

static void idac_xi_get_analogs(struct idac_io_analog_state *out) {
    XINPUT_STATE xi;
    int left;
    int right;

    assert(out != NULL);

    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);

    left = xi.Gamepad.sThumbLX;
    right = xi.Gamepad.sThumbRX;

    // normalize the steering axis
    left = calculate_norm_steering(left, idac_xi_left_stick_deadzone, idac_xi_linear_steering);
    right = calculate_norm_steering(right, idac_xi_right_stick_deadzone, idac_xi_linear_steering);

    if (idac_xi_single_stick_steering) {
        out->wheel = left;
    } else {
        out->wheel = (left + right) / 2;
    }

    out->accel = xi.Gamepad.bRightTrigger << 8;
    out->brake = xi.Gamepad.bLeftTrigger << 8;
}

static HRESULT idac_xi_ffb_init(void) {
    return S_OK;
}

static void idac_xi_ffb_toggle(bool active) {
    XINPUT_VIBRATION vibration;

    memset(&vibration, 0, sizeof(vibration));

    XInputSetState(0, &vibration);
}

static void idac_xi_ffb_constant_force(uint8_t direction, uint8_t force) {
    return;
}

static void idac_xi_ffb_rumble(uint8_t force, uint8_t period) {
    XINPUT_VIBRATION vibration;
    /* XInput max strength is 65.535, so multiply the 127.0 by 516. */
    uint16_t strength = force * 516;

    memset(&vibration, 0, sizeof(vibration));
    vibration.wLeftMotorSpeed = strength;
    vibration.wRightMotorSpeed = strength;

    XInputSetState(0, &vibration);
}

static void idac_xi_ffb_damper(uint8_t force) {
    return;
}
