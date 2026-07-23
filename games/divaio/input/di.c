#include <windows.h>
#include <assert.h>

#include "divaio/config.h"
#include "divaio/input/backend.h"
#include "divaio/input/di.h"
#include "divaio/input/di-dev.h"
#include "divaio/input/wnd.h"

#include "util/dprintf.h"
#include "util/str.h"

struct diva_di_axis {
    wchar_t name[4];
    size_t off;
};

static const struct diva_di_axis diva_di_axes[] = {
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


static HRESULT diva_di_config_apply(const struct diva_di_config* cfg);
static BOOL CALLBACK diva_di_enum_callback(
    const DIDEVICEINSTANCEW* dev,
    void* ctx);
static void diva_di_get_gamebtns(struct diva_button_states* states);
static void diva_di_get_opbtns(uint8_t* opbtn_out);
static uint8_t diva_di_decode_pov(DWORD pov);
static void diva_di_get_auto_status(bool* left, bool* right);

static const struct diva_io_backend diva_di_backend = {
    .get_gamebtns    = diva_di_get_gamebtns,
    .get_opbtns      = diva_di_get_opbtns,
    .get_auto_status = diva_di_get_auto_status,
};

static HWND diva_di_wnd;
static IDirectInput8W* diva_di_api;
static IDirectInputDevice8W* diva_di_dev;
static uint8_t diva_di_start;
static uint8_t diva_di_service;
static uint8_t diva_di_test;
static uint8_t diva_di_cross[2];
static uint8_t diva_di_triangle[2];
static uint8_t diva_di_square[2];
static uint8_t diva_di_circle[2];
static uint8_t diva_di_auto_button_left;
static uint8_t diva_di_auto_button_right;
static const struct diva_di_axis* diva_di_auto_axis_left = NULL;
static const struct diva_di_axis* diva_di_auto_axis_right = NULL;
static bool diva_di_auto_left_invert;
static bool diva_di_auto_right_invert;

static union diva_di_state state = {0};

HRESULT diva_di_init(
    const struct diva_di_config* cfg,
    HINSTANCE inst,
    const struct diva_io_backend** backend) {
    assert(cfg != NULL);
    assert(backend != NULL);

    *backend = NULL;

    HRESULT hr = diva_di_config_apply(cfg);

    if (FAILED(hr)) {
        return hr;
    }

    hr = diva_io_wnd_create(inst, &diva_di_wnd);

    if (FAILED(hr)) {
        return hr;
    }

    hr = DirectInput8Create(
        inst,
        DIRECTINPUT_VERSION,
        &IID_IDirectInput8W,
        (void**)&diva_di_api,
        NULL);

    if (FAILED(hr)) {
        dprintf("DirectInput: DirectInput8Create failed: %08x\n", (int)hr);
        return hr;
    }

    hr = IDirectInput8_EnumDevices(
        diva_di_api,
        DI8DEVCLASS_GAMECTRL,
        diva_di_enum_callback,
        (void *) cfg,
        DIEDFL_ATTACHEDONLY);

    if (FAILED(hr)) {
        dprintf("DirectInput: EnumDevices failed: %08x\n", (int)hr);

        return hr;
    }

    if (diva_di_dev == NULL) {
        dprintf("Stick: Controller not found\n");

        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    hr = diva_di_dev_start(diva_di_dev, diva_di_wnd);

    if (FAILED(hr)) {
        return hr;
    }

    dprintf("DirectInput: Controller initialized\n");

    *backend = &diva_di_backend;

    return S_OK;
}

static const struct diva_di_axis* diva_di_get_axis(const wchar_t* name) {
    for (int i = 0; i < _countof(diva_di_axes); i++) {
        const struct diva_di_axis* axis = &diva_di_axes[i];

        if (wstr_ieq(name, axis->name)) {
            return axis;
        }
    }

    return NULL;
}

static HRESULT diva_di_config_apply(const struct diva_di_config* cfg) {

    if (cfg->start > 32) {
        dprintf("Stick: Invalid start button: %i\n", cfg->start);
        return E_INVALIDARG;
    }

    for (int i = 0; i < 2; i++) {
        if (cfg->triangle[i] > 32) {
            dprintf("Stick: Invalid triangle button %d: %i\n", i+1, cfg->triangle[i]);
            return E_INVALIDARG;
        }

        if (cfg->square[i] > 32) {
            dprintf("Stick: Invalid square button %d: %i\n", i+1, cfg->square[i]);
            return E_INVALIDARG;
        }

        if (cfg->circle[i] > 32) {
            dprintf("Stick: Invalid circle button %d: %i\n", i+1, cfg->circle[i]);
            return E_INVALIDARG;
        }

        if (cfg->cross[i] > 32) {
            dprintf("Stick: Invalid cross button %d: %i\n", i+1, cfg->cross[i]);
            return E_INVALIDARG;
        }
    }

    if (cfg->auto_left_button > 32) {
        dprintf("Stick: Invalid auto slide left button: %i\n", cfg->auto_left_button);
        return E_INVALIDARG;
    }

    if (cfg->auto_right_button > 32) {
        dprintf("Stick: Invalid auto slide right button: %i\n", cfg->auto_right_button);
        return E_INVALIDARG;
    }

    diva_di_auto_axis_left = diva_di_get_axis(cfg->auto_left_axis);
    diva_di_auto_axis_right = diva_di_get_axis(cfg->auto_right_axis);

    /* Print some debug output to make sure config works... */

    dprintf("Stick: --- Begin configuration ---\n");
    dprintf("Stick: Device name  . . : Contains \"%S\"\n", cfg->device_name);
    dprintf("Stick: Start key  . . . : %u\n", cfg->start);
    for (int i = 0; i < 2; i++) {
        dprintf("Stick: Square key %d . . : %u\n", i+1, cfg->square[i]);
        dprintf("Stick: Triangle key %d . : %u\n", i+1, cfg->triangle[i]);
        dprintf("Stick: Cross key %d  . . : %u\n", i+1, cfg->cross[i]);
        dprintf("Stick: Circle key %d . . : %u\n", i+1, cfg->circle[i]);
    }
    dprintf("Stick: Left slide key . : %u\n", cfg->auto_left_button);
    dprintf("Stick: Right slide key  : %u\n", cfg->auto_right_button);
    dprintf("Stick: Left slide axis  : %ls\n", diva_di_auto_axis_left != NULL ? diva_di_auto_axis_left->name : L"Disabled");
    dprintf("Stick: Right slide axis : %ls\n", diva_di_auto_axis_right != NULL ? diva_di_auto_axis_right->name : L"Disabled");
    dprintf("Stick: --- End configuration ---\n");

    diva_di_start = cfg->start;
    diva_di_service = cfg->service;
    diva_di_test = cfg->test;

    diva_di_auto_button_left = cfg->auto_left_button;
    diva_di_auto_button_right = cfg->auto_right_button;
    diva_di_auto_left_invert = cfg->auto_left_invert;
    diva_di_auto_right_invert = cfg->auto_right_invert;

    for (int i = 0; i < 2; i++) {
        diva_di_cross[i] = cfg->cross[i];
        diva_di_circle[i] = cfg->circle[i];
        diva_di_triangle[i] = cfg->triangle[i];
        diva_di_square[i] = cfg->square[i];
    }

    return S_OK;
}

static BOOL CALLBACK diva_di_enum_callback(
    const DIDEVICEINSTANCEW* dev,
    void* ctx) {
    const struct diva_di_config* cfg = ctx;

    if (wcsstr(dev->tszProductName, cfg->device_name) == NULL) {
        return DIENUM_CONTINUE;
    }

    dprintf("Stick: Using DirectInput device \"%S\"\n", dev->tszProductName);

    HRESULT hr = IDirectInput8_CreateDevice(
        diva_di_api,
        &dev->guidInstance,
        &diva_di_dev,
        NULL);

    if (FAILED(hr)) {
        dprintf("Stick: CreateDevice failed: %08x\n", (int)hr);
    }

    return DIENUM_STOP;
}

static void diva_di_get_gamebtns(struct diva_button_states* states) {
    assert(states != NULL);

    HRESULT hr = diva_di_dev_poll(diva_di_dev, diva_di_wnd, &state);

    if (FAILED(hr)) {
        return;
    }

    uint16_t pov = diva_di_decode_pov(state.st.rgdwPOV[0]);

    states->start = diva_di_start && state.st.rgbButtons[diva_di_start - 1];
    states->cross.primary = diva_di_cross[0] && state.st.rgbButtons[diva_di_cross[0] - 1];
    states->cross.secondary = diva_di_cross[1] && state.st.rgbButtons[diva_di_cross[1] - 1];
    states->cross.secondary |= pov & DIVA_IO_GAMEBTN_CROSS;
    states->circle.primary = diva_di_circle[0] && state.st.rgbButtons[diva_di_circle[0] - 1];
    states->circle.secondary = diva_di_circle[1] && state.st.rgbButtons[diva_di_circle[1] - 1];
    states->circle.secondary |= pov & DIVA_IO_GAMEBTN_CIRCLE;
    states->square.primary = diva_di_square[0] && state.st.rgbButtons[diva_di_square[0] - 1];
    states->square.secondary = diva_di_square[1] && state.st.rgbButtons[diva_di_square[1] - 1];
    states->square.secondary |= pov & DIVA_IO_GAMEBTN_SQUARE;
    states->triangle.primary = diva_di_triangle[0] && state.st.rgbButtons[diva_di_triangle[0] - 1];
    states->triangle.secondary = diva_di_triangle[1] && state.st.rgbButtons[diva_di_triangle[1] - 1];
    states->triangle.secondary |= pov & DIVA_IO_GAMEBTN_TRIANGLE;
}

static uint8_t diva_di_decode_pov(DWORD pov) {
    switch (pov) {
    case 0: return DIVA_IO_GAMEBTN_TRIANGLE;
    case 4500: return DIVA_IO_GAMEBTN_TRIANGLE | DIVA_IO_GAMEBTN_CIRCLE;
    case 9000: return DIVA_IO_GAMEBTN_CIRCLE;
    case 13500: return DIVA_IO_GAMEBTN_CIRCLE | DIVA_IO_GAMEBTN_CROSS;
    case 18000: return DIVA_IO_GAMEBTN_CROSS;
    case 22500: return DIVA_IO_GAMEBTN_CROSS | DIVA_IO_GAMEBTN_SQUARE;
    case 27000: return DIVA_IO_GAMEBTN_SQUARE;
    case 31500: return DIVA_IO_GAMEBTN_SQUARE | DIVA_IO_GAMEBTN_TRIANGLE;
    default: return 0;
    }
}

void diva_di_get_auto_status(bool* left, bool* right) {
    assert(left != NULL);
    assert(right != NULL);

    *left = false;
    *right = false;

    if (diva_di_auto_button_left && state.st.rgbButtons[diva_di_auto_button_left - 1]) {
        *left = true;
    }
    if (diva_di_auto_button_right && state.st.rgbButtons[diva_di_auto_button_right - 1]) {
        *right = true;
    }

    // hardcoded deadzone is kinda ugly
    if (diva_di_auto_axis_left != NULL) {
        bool l = *(LONG*)&state.bytes[diva_di_auto_axis_left->off] < 0x5FFF;
        bool r = *(LONG*)&state.bytes[diva_di_auto_axis_left->off] > 0x9FFF;
        if (diva_di_auto_left_invert) {
            *left |= r;
            *right |= l;
        } else {
            *left |= l;
            *right |= r;
        }
    }
    if (diva_di_auto_axis_right != NULL) {
        bool l = *(LONG*)&state.bytes[diva_di_auto_axis_right->off] < 0x5FFF;
        bool r = *(LONG*)&state.bytes[diva_di_auto_axis_right->off] > 0x9FFF;
        if (diva_di_auto_right_invert) {
            *left |= r;
            *right |= l;
        } else {
            *left |= l;
            *right |= r;
        }
    }
}

static void diva_di_get_opbtns(uint8_t* opbtn_out) {
    uint8_t opbtn = 0;

    if (diva_di_service && state.st.rgbButtons[diva_di_service - 1]) {
        opbtn |= DIVA_IO_OPBTN_SERVICE;
    }

    if (diva_di_test && state.st.rgbButtons[diva_di_test - 1]) {
        opbtn |= DIVA_IO_OPBTN_TEST;
    }

    *opbtn_out = opbtn;
}
