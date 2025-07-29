#include <windows.h>
#include <assert.h>

#include "apm3io/backend.h"
#include "apm3io/config.h"
#include "apm3io/di.h"
#include "apm3io/di-dev.h"
#include "apm3io/apm3io.h"
#include "apm3io/wnd.h"

#include "util/dprintf.h"
#include "util/str.h"

struct apm3_di_axis {
    wchar_t name[4];
    size_t off;
};

static HRESULT apm3_di_config_apply(const struct apm3_di_config *cfg);
static BOOL CALLBACK apm3_di_enum_callback(
        const DIDEVICEINSTANCEW *dev,
        void *ctx);
static void apm3_di_get_gamebtns(uint16_t *gamebtn_out);
static uint8_t apm3_di_decode_pov(DWORD pov);


static const struct apm3_io_backend apm3_di_backend = {
    .get_gamebtns   = apm3_di_get_gamebtns,
};

static HWND apm3_di_wnd;
static IDirectInput8W *apm3_di_api;
static IDirectInputDevice8W *apm3_di_dev;
static IDirectInputEffect *apm3_di_fx;
static uint8_t apm3_di_home;
static uint8_t apm3_di_start;
static uint8_t apm3_di_button[APM3_BUTTON_COUNT];

HRESULT apm3_di_init(
        const struct apm3_di_config *cfg,
        HINSTANCE inst,
        const struct apm3_io_backend **backend)
{
    HRESULT hr;

    assert(cfg != NULL);
    assert(backend != NULL);

    *backend = NULL;

    hr = apm3_di_config_apply(cfg);

    if (FAILED(hr)) {
        return hr;
    }

    hr = apm3_io_wnd_create(inst, &apm3_di_wnd);

    if (FAILED(hr)) {
        return hr;
    }

    hr = DirectInput8Create(
            inst,
            DIRECTINPUT_VERSION,
            &IID_IDirectInput8W,
            (void**)&apm3_di_api,
            NULL);
    
    if (FAILED(hr)) {
        dprintf("DirectInput: DirectInput8Create failed: %08x\n", (int)hr);
        return hr;
    }

    hr = IDirectInput8_EnumDevices(
            apm3_di_api,
            DI8DEVCLASS_GAMECTRL,
            apm3_di_enum_callback,
            (void *) cfg,
            DIEDFL_ATTACHEDONLY);

    if (FAILED(hr)) {
        dprintf("DirectInput: EnumDevices failed: %08x\n", (int) hr);

        return hr;
    }

    if (apm3_di_dev == NULL) {
        dprintf("Stick: Controller not found\n");

        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    hr = apm3_di_dev_start(apm3_di_dev, apm3_di_wnd);

    if (FAILED(hr)) {
        return hr;
    }

    dprintf("DirectInput: Controller initialized\n");

    *backend = &apm3_di_backend;

    return S_OK;
}

static HRESULT apm3_di_config_apply(const struct apm3_di_config *cfg)
{
    int i;

    if (cfg->start > 32) {
        dprintf("Stick: Invalid start button: %i\n", cfg->start);

        return E_INVALIDARG;
    }

    if (cfg->home > 32) {
        dprintf("Stick: Invalid home button: %i\n", cfg->home);

        return E_INVALIDARG;
    }

    /* Check all 8 defined buttons */
    for (i = 0; i < 8; i++) {
        if (cfg->button[i] > 32) {
            dprintf("Stick: Invalid button %i: %i\n", i, cfg->button[i]);

            return E_INVALIDARG;
        }
    }

    /* Print some debug output to make sure config works... */

    dprintf("Stick: --- Begin configuration ---\n");
    dprintf("Stick: Device name . . . . : Contains \"%S\"\n",
            cfg->device_name);
    dprintf("Stick: Home button  . . . : %i\n", cfg->home);
    dprintf("Stick: Start button . . . : %i\n", cfg->start);

    /* Print the configuration for all 8 buttons */
    for (i = 0; i < APM3_BUTTON_COUNT; i++) {
        dprintf("Stick: Button %i . . . . . : %i\n", i, cfg->button[i]);
    }

    dprintf("Stick: --- End configuration ---\n");

    apm3_di_start = cfg->start;
    apm3_di_home = cfg->home;
    
    for (i = 0; i < APM3_BUTTON_COUNT; i++) {
        apm3_di_button[i] = cfg->button[i];
    }

    return S_OK;
}

static BOOL CALLBACK apm3_di_enum_callback(
        const DIDEVICEINSTANCEW *dev,
        void *ctx)
{
    const struct apm3_di_config *cfg;
    HRESULT hr;

    cfg = ctx;

    if (wcsstr(dev->tszProductName, cfg->device_name) == NULL) {
        return DIENUM_CONTINUE;
    }

    dprintf("Stick: Using DirectInput device \"%S\"\n", dev->tszProductName);

    hr = IDirectInput8_CreateDevice(
            apm3_di_api,
            &dev->guidInstance,
            &apm3_di_dev,
            NULL);

    if (FAILED(hr)) {
        dprintf("Stick: CreateDevice failed: %08x\n", (int) hr);
    }

    return DIENUM_STOP;
}

static void apm3_di_get_gamebtns(uint16_t *gamebtn_out)
{
    union apm3_di_state state;
    uint16_t gamebtn;
    HRESULT hr;

    assert(gamebtn_out != NULL);

    hr = apm3_di_dev_poll(apm3_di_dev, apm3_di_wnd, &state);

    if (FAILED(hr)) {
        return;
    }

    gamebtn = apm3_di_decode_pov(state.st.rgdwPOV[0]);

    if (apm3_di_start && state.st.rgbButtons[apm3_di_start - 1]) {
        gamebtn |= APM3_IO_GAMEBTN_START;
    }

    if (apm3_di_home && state.st.rgbButtons[apm3_di_home - 1]) {
        gamebtn |= APM3_IO_GAMEBTN_HOME;
    }

    if (apm3_di_button[0] && state.st.rgbButtons[apm3_di_button[0] - 1]) {
        gamebtn |= APM3_IO_GAMEBTN_B1;
    }

    if (apm3_di_button[1] && state.st.rgbButtons[apm3_di_button[1] - 1]) {
        gamebtn |= APM3_IO_GAMEBTN_B2;
    }

    if (apm3_di_button[2] && state.st.rgbButtons[apm3_di_button[2] - 1]) {
        gamebtn |= APM3_IO_GAMEBTN_B3;
    }

    if (apm3_di_button[3] && state.st.rgbButtons[apm3_di_button[3] - 1]) {
        gamebtn |= APM3_IO_GAMEBTN_B4;
    }

    if (apm3_di_button[4] && state.st.rgbButtons[apm3_di_button[4] - 1]) {
        gamebtn |= APM3_IO_GAMEBTN_B5;
    }

    if (apm3_di_button[5] && state.st.rgbButtons[apm3_di_button[5] - 1]) {
        gamebtn |= APM3_IO_GAMEBTN_B6;
    }

    if (apm3_di_button[6] && state.st.rgbButtons[apm3_di_button[6] - 1]) {
        gamebtn |= APM3_IO_GAMEBTN_B7;
    }

    if (apm3_di_button[7] && state.st.rgbButtons[apm3_di_button[7] - 1]) {
        gamebtn |= APM3_IO_GAMEBTN_B8;
    }

    *gamebtn_out = gamebtn;
}

static uint8_t apm3_di_decode_pov(DWORD pov)
{
    switch (pov) {
        case 0:     return APM3_IO_GAMEBTN_UP;
        case 4500:  return APM3_IO_GAMEBTN_UP | APM3_IO_GAMEBTN_RIGHT;
        case 9000:  return APM3_IO_GAMEBTN_RIGHT;
        case 13500: return APM3_IO_GAMEBTN_RIGHT | APM3_IO_GAMEBTN_DOWN;
        case 18000: return APM3_IO_GAMEBTN_DOWN;
        case 22500: return APM3_IO_GAMEBTN_DOWN | APM3_IO_GAMEBTN_LEFT;
        case 27000: return APM3_IO_GAMEBTN_LEFT;
        case 31500: return APM3_IO_GAMEBTN_LEFT | APM3_IO_GAMEBTN_UP;
        default:    return 0;
    }
}
