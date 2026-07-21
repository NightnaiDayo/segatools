#include <windows.h>
#include <assert.h>

#include "hkbio/backend.h"
#include "hkbio/config.h"
#include "hkbio/di.h"
#include "hkbio/di-dev.h"
#include "hkbio/wnd.h"

#include "util/dprintf.h"
#include "util/str.h"

static HRESULT hkb_di_config_apply(const struct hkb_di_config* cfg);
static BOOL CALLBACK hkb_di_enum_callback(const DIDEVICEINSTANCEW* dev, void* ctx);
static void hkb_di_get_gamebtns(uint16_t* gamebtn_out);
static void hkb_di_get_opbtns(uint8_t* opbtn_out);
static uint8_t hkb_di_decode_pov(DWORD pov);

static const struct hkb_io_backend hkb_di_backend = {
    .get_gamebtns = hkb_di_get_gamebtns,
    .get_opbtns   = hkb_di_get_opbtns
};

static HWND hkb_di_wnd;
static IDirectInput8W* hkb_di_api;
static IDirectInputDevice8W* hkb_di_dev;
static uint8_t hkb_di_decide;
static uint8_t hkb_di_cancel;
static uint8_t hkb_di_service;
static uint8_t hkb_di_test;
static uint8_t hkb_di_extra_up;
static uint8_t hkb_di_extra_right;
static uint8_t hkb_di_extra_down;
static uint8_t hkb_di_extra_left;
static uint8_t hkb_di_left_2;
static uint8_t hkb_di_right_2;

static union hkb_di_state state = {
    0
};

HRESULT hkb_di_init(const struct hkb_di_config* cfg, HINSTANCE inst, const struct hkb_io_backend** backend) {
    assert(cfg != NULL);
    assert(backend != NULL);

    *backend = NULL;

    HRESULT hr = hkb_di_config_apply(cfg);

    if (FAILED(hr)) {
        return hr;
    }

    hr = hkb_io_wnd_create(inst, &hkb_di_wnd);

    if (FAILED(hr)) {
        return hr;
    }

    hr = DirectInput8Create(inst, DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void**)&hkb_di_api, NULL);

    if (FAILED(hr)) {
        dprintf("DirectInput: DirectInput8Create failed: %08x\n", (int)hr);
        return hr;
    }

    hr = IDirectInput8_EnumDevices(hkb_di_api, DI8DEVCLASS_GAMECTRL, hkb_di_enum_callback, (void *) cfg, DIEDFL_ATTACHEDONLY);

    if (FAILED(hr)) {
        dprintf("DirectInput: EnumDevices failed: %08x\n", (int)hr);

        return hr;
    }

    if (hkb_di_dev == NULL) {
        dprintf("Stick: Controller not found\n");

        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    hr = hkb_di_dev_start(hkb_di_dev, hkb_di_wnd);

    if (FAILED(hr)) {
        return hr;
    }

    dprintf("DirectInput: Controller initialized\n");

    *backend = &hkb_di_backend;

    return S_OK;
}

static HRESULT hkb_di_config_apply(const struct hkb_di_config* cfg) {
    if (cfg->decide > 32) {
        dprintf("Stick: Invalid start button: %i\n", cfg->decide);

        return E_INVALIDARG;
    }

    if (cfg->cancel > 32) {
        dprintf("Stick: Invalid home button: %i\n", cfg->cancel);

        return E_INVALIDARG;
    }

    if (cfg->left_2 > 32) {
        dprintf("Stick: Invalid home button: %i\n", cfg->left_2);

        return E_INVALIDARG;
    }

    if (cfg->right_2 > 32) {
        dprintf("Stick: Invalid home button: %i\n", cfg->right_2);

        return E_INVALIDARG;
    }

    /* Print some debug output to make sure config works... */

    dprintf("Stick: --- Begin configuration ---\n");
    dprintf("Stick: Device name . . . . : Contains \"%S\"\n", cfg->device_name);
    dprintf("Stick: Decision button  . : %i\n", cfg->decide);
    dprintf("Stick: Cancel button  . . : %i\n", cfg->cancel);
    dprintf("Stick: Left page button . : %i\n", cfg->left_2);
    dprintf("Stick: Right page button  : %i\n", cfg->right_2);
    dprintf("Stick: Service button . . : %i\n", cfg->service);
    dprintf("Stick: Test button  . . . : %i\n", cfg->test);
    dprintf("Stick: --- End configuration ---\n");

    hkb_di_decide = cfg->decide;
    hkb_di_cancel = cfg->cancel;
    hkb_di_left_2 = cfg->left_2;
    hkb_di_right_2 = cfg->right_2;
    hkb_di_service = cfg->service;
    hkb_di_test = cfg->test;

    hkb_di_extra_up = cfg->up;
    hkb_di_extra_right = cfg->right;
    hkb_di_extra_down = cfg->down;
    hkb_di_extra_left = cfg->left;

    return S_OK;
}

static BOOL CALLBACK hkb_di_enum_callback(const DIDEVICEINSTANCEW* dev, void* ctx) {
    const struct hkb_di_config* cfg = ctx;

    if (wcsstr(dev->tszProductName, cfg->device_name) == NULL) {
        return DIENUM_CONTINUE;
    }

    dprintf("Stick: Using DirectInput device \"%S\"\n", dev->tszProductName);

    HRESULT hr = IDirectInput8_CreateDevice(hkb_di_api, &dev->guidInstance, &hkb_di_dev, NULL);

    if (FAILED(hr)) {
        dprintf("Stick: CreateDevice failed: %08x\n", (int)hr);
    }

    return DIENUM_STOP;
}

static void hkb_di_get_gamebtns(uint16_t* gamebtn_out) {
    assert(gamebtn_out != NULL);

    HRESULT hr = hkb_di_dev_poll(hkb_di_dev, hkb_di_wnd, &state);

    if (FAILED(hr)) {
        return;
    }

    uint16_t gamebtn = hkb_di_decode_pov(state.st.rgdwPOV[0]);

    if (hkb_di_extra_up && state.st.rgbButtons[hkb_di_extra_up - 1]) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_UP;
    }

    if (hkb_di_extra_right && state.st.rgbButtons[hkb_di_extra_right - 1]) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_RIGHT;
    }

    if (hkb_di_extra_down && state.st.rgbButtons[hkb_di_extra_down - 1]) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_DOWN;
    }

    if (hkb_di_extra_left && state.st.rgbButtons[hkb_di_extra_left - 1]) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_LEFT;
    }

    if (hkb_di_decide && state.st.rgbButtons[hkb_di_decide - 1]) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_DECIDE;
    }

    if (hkb_di_cancel && state.st.rgbButtons[hkb_di_cancel - 1]) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_CANCEL;
    }

    if (hkb_di_left_2 && state.st.rgbButtons[hkb_di_left_2 - 1]) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_LEFT_2;
    }

    if (hkb_di_right_2 && state.st.rgbButtons[hkb_di_right_2 - 1]) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_RIGHT_2;
    }

    *gamebtn_out = gamebtn;
}

static uint8_t hkb_di_decode_pov(DWORD pov) {
    switch (pov) {
    case 0:
        return HKB_IO_GAMEBTN_TERMINAL_UP;
    case 4500:
        return HKB_IO_GAMEBTN_TERMINAL_UP | HKB_IO_GAMEBTN_TERMINAL_RIGHT;
    case 9000:
        return HKB_IO_GAMEBTN_TERMINAL_RIGHT;
    case 13500:
        return HKB_IO_GAMEBTN_TERMINAL_RIGHT | HKB_IO_GAMEBTN_TERMINAL_DOWN;
    case 18000:
        return HKB_IO_GAMEBTN_TERMINAL_DOWN;
    case 22500:
        return HKB_IO_GAMEBTN_TERMINAL_DOWN | HKB_IO_GAMEBTN_TERMINAL_LEFT;
    case 27000:
        return HKB_IO_GAMEBTN_TERMINAL_LEFT;
    case 31500:
        return HKB_IO_GAMEBTN_TERMINAL_LEFT | HKB_IO_GAMEBTN_TERMINAL_UP;
    default:
        return 0;
    }
}

static void hkb_di_get_opbtns(uint8_t* opbtn_out) {
    uint8_t opbtn = 0;

    if (hkb_di_service && state.st.rgbButtons[hkb_di_service - 1]) {
        opbtn |= HKB_IO_OPBTN_SERVICE;
    }
    if (hkb_di_test && state.st.rgbButtons[hkb_di_test - 1]) {
        opbtn |= HKB_IO_OPBTN_TEST;
    }

    *opbtn_out = opbtn;
}
