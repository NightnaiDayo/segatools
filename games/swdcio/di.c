#include <windows.h>
#include <assert.h>

#include "swdcio/backend.h"
#include "swdcio/config.h"
#include "swdcio/di.h"
#include "swdcio/di-dev.h"
#include "swdcio/swdcio.h"
#include "swdcio/wnd.h"

#include "util/dprintf.h"
#include "util/str.h"

struct swdc_di_axis {
    wchar_t name[4];
    size_t off;
};

static HRESULT swdc_di_config_apply(const struct swdc_di_config *cfg);
static const struct swdc_di_axis *swdc_di_get_axis(const wchar_t *name);
static BOOL CALLBACK swdc_di_enum_callback(
        const DIDEVICEINSTANCEW *dev,
        void *ctx);
static BOOL CALLBACK swdc_di_enum_callback_pedals(
        const DIDEVICEINSTANCEW *dev,
        void *ctx);
static void swdc_di_get_buttons(uint16_t *gamebtn_out);
static uint8_t swdc_di_decode_pov(DWORD pov);
static void swdc_di_get_analogs(struct swdc_io_analog_state *out);

static const struct swdc_di_axis swdc_di_axes[] = {
    /* Just map DIJOYSTATE for now, we can map DIJOYSTATE2 later if needed */
    { .name = L"X",     .off = DIJOFS_X },
    { .name = L"Y",     .off = DIJOFS_Y },
    { .name = L"Z",     .off = DIJOFS_Z },
    { .name = L"RX",    .off = DIJOFS_RX },
    { .name = L"RY",    .off = DIJOFS_RY },
    { .name = L"RZ",    .off = DIJOFS_RZ },
    { .name = L"U",     .off = DIJOFS_SLIDER(0) },
    { .name = L"V",     .off = DIJOFS_SLIDER(1) },
};

static const struct swdc_io_backend swdc_di_backend = {
    .get_gamebtns       = swdc_di_get_buttons,
    .get_analogs        = swdc_di_get_analogs,
    .ffb_init           = swdc_di_ffb_init,
    .ffb_toggle         = swdc_di_ffb_toggle,
    .ffb_constant_force = swdc_di_ffb_constant_force,
    .ffb_rumble         = swdc_di_ffb_rumble,
    .ffb_damper         = swdc_di_ffb_damper
};

static HWND swdc_di_wnd;
static IDirectInput8W *swdc_di_api;
static IDirectInputDevice8W *swdc_di_dev;
static IDirectInputDevice8W *swdc_di_pedals;
static IDirectInputEffect *swdc_di_fx;
static size_t swdc_di_off_brake;
static size_t swdc_di_off_accel;
static uint8_t swdc_di_paddle_left;
static uint8_t swdc_di_paddle_right;
static uint8_t swdc_di_view_chg;
static uint8_t swdc_di_start;
static uint8_t swdc_di_up;
static uint8_t swdc_di_down;
static uint8_t swdc_di_left;
static uint8_t swdc_di_right;
static uint8_t swdc_di_wheel_green;
static uint8_t swdc_di_wheel_red;
static uint8_t swdc_di_wheel_blue;
static uint8_t swdc_di_wheel_yellow;
static bool swdc_di_use_pedals;
static bool swdc_di_reverse_brake_axis;
static bool swdc_di_reverse_accel_axis;

HRESULT swdc_di_init(
        const struct swdc_di_config *cfg,
        HINSTANCE inst,
        const struct swdc_io_backend **backend)
{
    HRESULT hr;

    assert(cfg != NULL);
    assert(backend != NULL);

    *backend = NULL;

    hr = swdc_di_config_apply(cfg);

    if (FAILED(hr)) {
        return hr;
    }

    hr = swdc_io_wnd_create(inst, &swdc_di_wnd);

    if (FAILED(hr)) {
        return hr;
    }

    hr = DirectInput8Create(
            inst,
            DIRECTINPUT_VERSION,
            &IID_IDirectInput8W,
            (void**)&swdc_di_api,
            NULL);
    
    if (FAILED(hr)) {
        dprintf("DirectInput: DirectInput8Create failed: %08x\n", (int) hr);

        return hr;
    }

    hr = IDirectInput8_EnumDevices(
            swdc_di_api,
            DI8DEVCLASS_GAMECTRL,
            swdc_di_enum_callback,
            (void *) cfg,
            DIEDFL_ATTACHEDONLY);

    if (FAILED(hr)) {
        dprintf("DirectInput: EnumDevices failed: %08x\n", (int) hr);

        return hr;
    }

    if (swdc_di_dev == NULL) {
        dprintf("Wheel: Controller not found\n");

        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    hr = swdc_di_dev_init(cfg, swdc_di_dev, swdc_di_wnd);

    if (FAILED(hr)) {
        return hr;
    }

    if (cfg->pedals_name[0] != L'\0') {
        hr = IDirectInput8_EnumDevices(
                swdc_di_api,
                DI8DEVCLASS_GAMECTRL,
                swdc_di_enum_callback_pedals,
                (void *) cfg,
                DIEDFL_ATTACHEDONLY);

        if (FAILED(hr)) {
            dprintf("DirectInput: EnumDevices failed: %08x\n", (int) hr);

            return hr;
        }

        if (swdc_di_dev == NULL) {
            dprintf("Pedals: Controller not found\n");

            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }

        hr = swdc_di_dev_start(swdc_di_pedals, swdc_di_wnd);

        if (FAILED(hr)) {
            return hr;
        }

        swdc_di_use_pedals = true;
    } else {
        swdc_di_use_pedals = false;
    }

    dprintf("DirectInput: Controller initialized\n");

    *backend = &swdc_di_backend;

    return S_OK;
}

static HRESULT swdc_di_config_apply(const struct swdc_di_config *cfg)
{
    const struct swdc_di_axis *brake_axis;
    const struct swdc_di_axis *accel_axis;
    int i;

    brake_axis = swdc_di_get_axis(cfg->brake_axis);
    accel_axis = swdc_di_get_axis(cfg->accel_axis);

    if (brake_axis == NULL) {
        dprintf("Wheel: Invalid brake axis: %S\n", cfg->brake_axis);

        return E_INVALIDARG;
    }

    if (accel_axis == NULL) {
        dprintf("Wheel: Invalid accel axis: %S\n", cfg->accel_axis);

        return E_INVALIDARG;
    }

    if (cfg->start > 32) {
        dprintf("Wheel: Invalid start button: %i\n", cfg->start);

        return E_INVALIDARG;
    }

    if (cfg->view_chg > 32) {
        dprintf("Wheel: Invalid view change button: %i\n", cfg->view_chg);

        return E_INVALIDARG;
    }

    if (cfg->up > 32) {
        dprintf("Wheel: Invalid up button: %i\n", cfg->up);

        return E_INVALIDARG;
    }

    if (cfg->down > 32) {
        dprintf("Wheel: Invalid down button: %i\n", cfg->down);
        
        return E_INVALIDARG;
    }

    if (cfg->left > 32) {
        dprintf("Wheel: Invalid left button: %i\n", cfg->left);

        return E_INVALIDARG;
    }

    if (cfg->right > 32) {
        dprintf("Wheel: Invalid right button: %i\n", cfg->right);

        return E_INVALIDARG;
    }

    if (cfg->paddle_left > 32) {
        dprintf("Wheel: Invalid left paddle button: %i\n", cfg->paddle_left);

        return E_INVALIDARG;
    }

    if (cfg->paddle_right > 32) {
        dprintf("Wheel: Invalid right paddle button: %i\n", cfg->paddle_right);

        return E_INVALIDARG;
    }

    if (cfg->wheel_green > 32) {
        dprintf("Wheel: Invalid steering wheel green button: %i\n", cfg->wheel_green);

        return E_INVALIDARG;
    }
    
    if (cfg->wheel_red > 32) {
        dprintf("Wheel: Invalid steering wheel red button: %i\n", cfg->wheel_red);

        return E_INVALIDARG;
    }
    
    if (cfg->wheel_blue > 32) {
        dprintf("Wheel: Invalid steering wheel blue button: %i\n", cfg->wheel_blue);

        return E_INVALIDARG;
    }
    
    if (cfg->wheel_yellow > 32) {
        dprintf("Wheel: Invalid steering wheel yellow button: %i\n", cfg->wheel_yellow);

        return E_INVALIDARG;
    }

    /* Print some debug output to make sure config works... */

    dprintf("Wheel: --- Begin configuration ---\n");
    dprintf("Wheel: Device name . . . . : Contains \"%S\"\n",
            cfg->device_name);
    if (cfg->pedals_name[0] == L'\0') {
        dprintf("Wheel: Brake axis  . . . . : %S\n", brake_axis->name);
        dprintf("Wheel: Accel axis  . . . . : %S\n", accel_axis->name);
    }
    dprintf("Wheel: Start button . . . . . : %i\n", cfg->start);
    dprintf("Wheel: View Change button . . : %i\n", cfg->view_chg);
    dprintf("Wheel: Up button  . . . . . . : %i\n", cfg->up);
    dprintf("Wheel: Down button  . . . . . : %i\n", cfg->down);
    dprintf("Wheel: Left button  . . . . . : %i\n", cfg->left);
    dprintf("Wheel: Right button   . . . . : %i\n", cfg->right);
    dprintf("Wheel: Paddle Left button . . : %i\n", cfg->paddle_left);
    dprintf("Wheel: Paddle Right button  . : %i\n", cfg->paddle_right);
    dprintf("Wheel: Steering Green button  : %i\n", cfg->wheel_green);
    dprintf("Wheel: Steering Red button  . : %i\n", cfg->wheel_red);
    dprintf("Wheel: Steering Blue button . : %i\n", cfg->wheel_blue);
    dprintf("Wheel: Steering Yellow button : %i\n", cfg->wheel_yellow);
    dprintf("Wheel: Reverse Brake Axis . . : %i\n", cfg->reverse_brake_axis);
    dprintf("Wheel: Reverse Accel Axis . . : %i\n", cfg->reverse_accel_axis);
    dprintf("Wheel: ---  End  configuration ---\n");

    if (cfg->pedals_name[0] != L'\0') {
        dprintf("Pedals: --- Begin configuration ---\n");
        dprintf("Pedals: Device name . . . : Contains \"%S\"\n",
                cfg->pedals_name);
        dprintf("Pedals: Brake axis  . . . . : %S\n", brake_axis->name);
        dprintf("Pedals: Accel axis  . . . . : %S\n", accel_axis->name);
        dprintf("Pedals: ---  End  configuration ---\n");
    }

    swdc_di_off_brake = brake_axis->off;
    swdc_di_off_accel = accel_axis->off;
    swdc_di_start = cfg->start;
    swdc_di_view_chg = cfg->view_chg;
    swdc_di_up = cfg->up;
    swdc_di_down = cfg->down;
    swdc_di_left = cfg->left;
    swdc_di_right = cfg->right;
    swdc_di_paddle_left = cfg->paddle_left;
    swdc_di_paddle_right = cfg->paddle_right;
    swdc_di_wheel_green = cfg->wheel_green;
    swdc_di_wheel_red = cfg->wheel_red;
    swdc_di_wheel_blue = cfg->wheel_blue;
    swdc_di_wheel_yellow = cfg->wheel_yellow;
    swdc_di_reverse_brake_axis = cfg->reverse_brake_axis;
    swdc_di_reverse_accel_axis = cfg->reverse_accel_axis;

    /* FFB configuration */
    if (cfg->ffb_constant_force_strength < 0 || cfg->ffb_constant_force_strength > 100) {
        dprintf("Wheel: Invalid constant force strength: %i\n", cfg->ffb_constant_force_strength);

        return E_INVALIDARG;
    }
    
    if (cfg->ffb_rumble_strength < 0 || cfg->ffb_rumble_strength > 100) {
        dprintf("Wheel: Invalid rumble strength: %i\n", cfg->ffb_rumble_strength);

        return E_INVALIDARG;
    }

    if (cfg->ffb_damper_strength < 0 || cfg->ffb_damper_strength > 100) {
        dprintf("Wheel: Invalid damper strength: %i\n", cfg->ffb_damper_strength);

        return E_INVALIDARG;
    }

    return S_OK;
}

static const struct swdc_di_axis *swdc_di_get_axis(const wchar_t *name)
{
    const struct swdc_di_axis *axis;
    size_t i;

    for (i = 0 ; i < _countof(swdc_di_axes) ; i++) {
        axis = &swdc_di_axes[i];

        if (wstr_ieq(name, axis->name)) {
            return axis;
        }
    }

    return NULL;
}

static BOOL CALLBACK swdc_di_enum_callback(
        const DIDEVICEINSTANCEW *dev,
        void *ctx)
{
    const struct swdc_di_config *cfg;
    HRESULT hr;

    cfg = ctx;

    if (wcsstr(dev->tszProductName, cfg->device_name) == NULL) {
        return DIENUM_CONTINUE;
    }

    dprintf("Wheel: Using DirectInput device \"%S\"\n", dev->tszProductName);

    hr = IDirectInput8_CreateDevice(
            swdc_di_api,
            &dev->guidInstance,
            &swdc_di_dev,
            NULL);

    if (FAILED(hr)) {
        dprintf("Wheel: CreateDevice failed: %08x\n", (int) hr);
    }

    return DIENUM_STOP;
}

static BOOL CALLBACK swdc_di_enum_callback_pedals(
        const DIDEVICEINSTANCEW *dev,
        void *ctx)
{
    const struct swdc_di_config *cfg;
    HRESULT hr;

    cfg = ctx;

    if (wcsstr(dev->tszProductName, cfg->pedals_name) == NULL) {
        return DIENUM_CONTINUE;
    }

    dprintf("Pedals: Using DirectInput device \"%S\"\n", dev->tszProductName);

    hr = IDirectInput8_CreateDevice(
            swdc_di_api,
            &dev->guidInstance,
            &swdc_di_pedals,
            NULL);

    if (FAILED(hr)) {
        dprintf("Pedals: CreateDevice failed: %08x\n", (int) hr);
    }

    return DIENUM_STOP;
}

static void swdc_di_get_buttons(uint16_t *gamebtn_out)
{
    union swdc_di_state state;
    uint16_t gamebtn;
    HRESULT hr;

    assert(gamebtn_out != NULL);

    hr = swdc_di_dev_poll(swdc_di_dev, swdc_di_wnd, &state);

    if (FAILED(hr)) {
        return;
    }

    gamebtn = swdc_di_decode_pov(state.st.rgdwPOV[0]);

    if (swdc_di_start && state.st.rgbButtons[swdc_di_start - 1]) {
        gamebtn |= SWDC_IO_GAMEBTN_START;
    }

    if (swdc_di_view_chg && state.st.rgbButtons[swdc_di_view_chg - 1]) {
        gamebtn |= SWDC_IO_GAMEBTN_VIEW_CHANGE;
    }

    if (swdc_di_up && state.st.rgbButtons[swdc_di_up - 1]) {
        gamebtn |= SWDC_IO_GAMEBTN_UP;
    }

    if (swdc_di_down && state.st.rgbButtons[swdc_di_down - 1]) {
        gamebtn |= SWDC_IO_GAMEBTN_DOWN;
    }

    if (swdc_di_left && state.st.rgbButtons[swdc_di_left - 1]) {
        gamebtn |= SWDC_IO_GAMEBTN_LEFT;
    }

    if (swdc_di_right && state.st.rgbButtons[swdc_di_right - 1]) {
        gamebtn |= SWDC_IO_GAMEBTN_RIGHT;
    }

    if (swdc_di_paddle_left && state.st.rgbButtons[swdc_di_paddle_left - 1]) {
        gamebtn |= SWDC_IO_GAMEBTN_STEERING_PADDLE_LEFT;
    }

    if (swdc_di_paddle_right && state.st.rgbButtons[swdc_di_paddle_right - 1]) {
        gamebtn |= SWDC_IO_GAMEBTN_STEERING_PADDLE_RIGHT;
    }

    if (swdc_di_wheel_green && state.st.rgbButtons[swdc_di_wheel_green - 1]) {
        gamebtn |= SWDC_IO_GAMEBTN_STEERING_GREEN;
    }

    if (swdc_di_wheel_red && state.st.rgbButtons[swdc_di_wheel_red - 1]) {
        gamebtn |= SWDC_IO_GAMEBTN_STEERING_RED;
    }

    if (swdc_di_wheel_blue && state.st.rgbButtons[swdc_di_wheel_blue - 1]) {
        gamebtn |= SWDC_IO_GAMEBTN_STEERING_BLUE;
    }

    if (swdc_di_wheel_yellow && state.st.rgbButtons[swdc_di_wheel_yellow - 1]) {
        gamebtn |= SWDC_IO_GAMEBTN_STEERING_YELLOW;
    }

    *gamebtn_out = gamebtn;
}

static uint8_t swdc_di_decode_pov(DWORD pov)
{
    switch (pov) {
        case 0:     return SWDC_IO_GAMEBTN_UP;
        case 4500:  return SWDC_IO_GAMEBTN_UP | SWDC_IO_GAMEBTN_RIGHT;
        case 9000:  return SWDC_IO_GAMEBTN_RIGHT;
        case 13500: return SWDC_IO_GAMEBTN_RIGHT | SWDC_IO_GAMEBTN_DOWN;
        case 18000: return SWDC_IO_GAMEBTN_DOWN;
        case 22500: return SWDC_IO_GAMEBTN_DOWN | SWDC_IO_GAMEBTN_LEFT;
        case 27000: return SWDC_IO_GAMEBTN_LEFT;
        case 31500: return SWDC_IO_GAMEBTN_LEFT | SWDC_IO_GAMEBTN_UP;
        default:    return 0;
    }
}

static void swdc_di_get_analogs(struct swdc_io_analog_state *out)
{
    union swdc_di_state state;
    union swdc_di_state pedals_state;
    const LONG *brake;
    const LONG *accel;
    HRESULT hr;

    assert(out != NULL);

    hr = swdc_di_dev_poll(swdc_di_dev, swdc_di_wnd, &state);

    if (FAILED(hr)) {
        return;
    }

    if (swdc_di_use_pedals) {
        hr = swdc_di_dev_poll(swdc_di_pedals, swdc_di_wnd, &pedals_state);

        if (FAILED(hr)) {
            return;
        }

        brake = (LONG *) &pedals_state.bytes[swdc_di_off_brake];
        accel = (LONG *) &pedals_state.bytes[swdc_di_off_accel];
    } else {
        brake = (LONG *) &state.bytes[swdc_di_off_brake];
        accel = (LONG *) &state.bytes[swdc_di_off_accel];
    }

    out->wheel = state.st.lX - 32768;

    if (swdc_di_reverse_brake_axis) {
        out->brake = *brake;
    } else {
        out->brake = 65535 - *brake;
    }

    if (swdc_di_reverse_accel_axis) {
        out->accel = *accel;
    } else {
        out->accel = 65535 - *accel;
    }
}
