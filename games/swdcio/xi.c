#include <windows.h>
#include <xinput.h>

#include <math.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "swdcio/backend.h"
#include "swdcio/config.h"
#include "swdcio/swdcio.h"
#include "swdcio/xi.h"

#include "util/dprintf.h"

static void swdc_xi_get_gamebtns(uint16_t *gamebtn_out);
static void swdc_xi_get_analogs(struct swdc_io_analog_state *out);

static HRESULT swdc_xi_ffb_init(void);
static void swdc_xi_ffb_toggle(bool active);
static void swdc_xi_ffb_constant_force(uint8_t direction, uint8_t force);
static void swdc_xi_ffb_rumble(uint8_t force, uint8_t period);
static void swdc_xi_ffb_damper(uint8_t force);

static HRESULT swdc_xi_config_apply(const struct swdc_xi_config *cfg);

static const struct swdc_io_backend swdc_xi_backend = {
    .get_gamebtns       = swdc_xi_get_gamebtns,
    .get_analogs        = swdc_xi_get_analogs,
    .ffb_init           = swdc_xi_ffb_init,
    .ffb_toggle         = swdc_xi_ffb_toggle,
    .ffb_constant_force = swdc_xi_ffb_constant_force,
    .ffb_rumble         = swdc_xi_ffb_rumble,
    .ffb_damper         = swdc_xi_ffb_damper
};

static bool swdc_xi_single_stick_steering;
static bool swdc_xi_linear_steering;
static uint16_t swdc_xi_left_stick_deadzone;
static uint16_t swdc_xi_right_stick_deadzone;

const uint16_t max_stick_value = 32767;
/* Apply steering wheel restriction. Real cabs only report about 76% of
    the output value when the wheel is turned to either of its maximum positions. */
const uint16_t max_wheel_value = 24831;

HRESULT swdc_xi_init(const struct swdc_xi_config *cfg, const struct swdc_io_backend **backend)
{
    HRESULT hr;
    assert(cfg != NULL);
    assert(backend != NULL);

    hr = swdc_xi_config_apply(cfg);

    if (FAILED(hr)) {
        return hr;
    }

    dprintf("XInput: Using XInput controller\n");
    *backend = &swdc_xi_backend;

    return S_OK;
}

HRESULT swdc_io_poll(void)
{    
    return S_OK;
}

static HRESULT swdc_xi_config_apply(const struct swdc_xi_config *cfg) {
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

    swdc_xi_single_stick_steering = cfg->single_stick_steering;
    swdc_xi_linear_steering = cfg->linear_steering;
    swdc_xi_left_stick_deadzone = cfg->left_stick_deadzone;
    swdc_xi_right_stick_deadzone = cfg->right_stick_deadzone;

    return S_OK;
}

static void swdc_xi_get_gamebtns(uint16_t *gamebtn_out)
{
    uint16_t gamebtn;
    XINPUT_STATE xi;
    WORD xb;

    assert(gamebtn_out != NULL);

    gamebtn = 0;

    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);
    xb = xi.Gamepad.wButtons;

    if (xb & XINPUT_GAMEPAD_DPAD_UP) {
        gamebtn |= SWDC_IO_GAMEBTN_UP;
    }

    if (xb & XINPUT_GAMEPAD_DPAD_DOWN) {
        gamebtn |= SWDC_IO_GAMEBTN_DOWN;
    }

    if (xb & XINPUT_GAMEPAD_DPAD_LEFT) {
        gamebtn |= SWDC_IO_GAMEBTN_LEFT;
    }

    if (xb & XINPUT_GAMEPAD_DPAD_RIGHT) {
        gamebtn |= SWDC_IO_GAMEBTN_RIGHT;
    }

    if (xb & XINPUT_GAMEPAD_START) {
        gamebtn |= SWDC_IO_GAMEBTN_START;
    }

    if (xb & XINPUT_GAMEPAD_BACK) {
        gamebtn |= SWDC_IO_GAMEBTN_VIEW_CHANGE;
    }

    if (xb & XINPUT_GAMEPAD_A) {
        gamebtn |= SWDC_IO_GAMEBTN_STEERING_GREEN;
    }

    if (xb & XINPUT_GAMEPAD_B) {
        gamebtn |= SWDC_IO_GAMEBTN_STEERING_RED;
    }

    if (xb & XINPUT_GAMEPAD_X) {
        gamebtn |= SWDC_IO_GAMEBTN_STEERING_BLUE;
    }

    if (xb & XINPUT_GAMEPAD_Y) {
        gamebtn |= SWDC_IO_GAMEBTN_STEERING_YELLOW;
    }

    if (xb & XINPUT_GAMEPAD_LEFT_SHOULDER) {
        gamebtn |= SWDC_IO_GAMEBTN_STEERING_PADDLE_LEFT;
    }

    if (xb & XINPUT_GAMEPAD_RIGHT_SHOULDER) {
        gamebtn |= SWDC_IO_GAMEBTN_STEERING_PADDLE_RIGHT;
    }

    *gamebtn_out = gamebtn;
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
        norm_magnitude = (magnitude / (max_stick_value - deadzone));
    } else // if the controller is in the deadzone zero out the magnitude
    {
        magnitude = 0.0;
        norm_magnitude = 0.0;
    }

    // apply non-linear transform to the axis
    if (!linear_steering) {
        return (int16_t)(norm_axis * powf(norm_magnitude, 3.0f) * max_wheel_value);
    }

    return (int16_t)(norm_axis * norm_magnitude * max_wheel_value);
}

static void swdc_xi_get_analogs(struct swdc_io_analog_state *out)
{
    XINPUT_STATE xi;
    int left;
    int right;

    assert(out != NULL);

    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);

    left = xi.Gamepad.sThumbLX;
    right = xi.Gamepad.sThumbRX;

    // normalize the steering axis
    left = calculate_norm_steering(left, swdc_xi_left_stick_deadzone, swdc_xi_linear_steering);
    right = calculate_norm_steering(right, swdc_xi_right_stick_deadzone, swdc_xi_linear_steering);
    
    if (swdc_xi_single_stick_steering) {
        out->wheel = left;
    } else {
        out->wheel = (left + right) / 2;
    }

    out->accel = xi.Gamepad.bRightTrigger << 8;
    out->brake = xi.Gamepad.bLeftTrigger << 8;
}

static HRESULT swdc_xi_ffb_init(void) {
    return S_OK;
}

static void swdc_xi_ffb_toggle(bool active) {
    XINPUT_VIBRATION vibration;

    memset(&vibration, 0, sizeof(vibration));

    XInputSetState(0, &vibration);
}

static void swdc_xi_ffb_constant_force(uint8_t direction, uint8_t force) {
    return;
}

static void swdc_xi_ffb_rumble(uint8_t force, uint8_t period) {
    XINPUT_VIBRATION vibration;
    /* XInput max strength is 65.535, so multiply the 127.0 by 516. */
    uint16_t strength = force * 516;

    memset(&vibration, 0, sizeof(vibration));
    vibration.wLeftMotorSpeed = strength;
    vibration.wRightMotorSpeed = strength;

    XInputSetState(0, &vibration);
}

static void swdc_xi_ffb_damper(uint8_t force) {
    return;
}
