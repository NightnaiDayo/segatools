#include <assert.h>
#include <dinput.h>
#include <stdbool.h>
#include <windows.h>

#include "idacio/di-dev.h"


#include "util/dprintf.h"

/* Globals */
static HWND idac_di_wnd = NULL;
static IDirectInputDevice8W* idac_di_dev = NULL;

static void update_or_create_effect(LPDIRECTINPUTEFFECT* effect_ptr, REFGUID guid, DIEFFECT* fx);

/* Effect handles */
static IDirectInputEffect* idac_di_fx_constant = NULL;
static IDirectInputEffect* idac_di_fx_rumble = NULL;
static IDirectInputEffect* idac_di_fx_spring = NULL;
static IDirectInputEffect* idac_di_fx_damper = NULL;

/* Max FFB Board value is 127 */
static const double idac_di_ffb_scale = 127.0;
static const double idac_di_damper_spring_ratio = 0.5;

/* Clamped config settings */
static uint8_t idac_di_constant_force_strength;
static uint8_t idac_di_rumble_strength;
static uint8_t idac_di_damper_strength;
static uint8_t idac_di_rumble_duration;
static uint8_t idac_di_base_damper;
static uint8_t idac_di_deadband;

HRESULT idac_di_dev_init(const struct idac_di_config* cfg,
                         IDirectInputDevice8W* dev, HWND wnd) {
    assert(cfg != NULL);
    assert(dev != NULL);
    assert(wnd != NULL);

    idac_di_dev = dev;
    idac_di_wnd = wnd;

    idac_di_rumble_duration = cfg->ffb_rumble_duration;

    idac_di_constant_force_strength = cfg->ffb_constant_force_strength > 100
                                        ? 100
                                        : cfg->ffb_constant_force_strength;

    idac_di_rumble_strength = cfg->ffb_rumble_strength > 100 
                                ? 100 
                                : cfg->ffb_rumble_strength;
    
    idac_di_damper_strength = cfg->ffb_damper_strength > 100
                                ? 100
                                : cfg->ffb_damper_strength;
    
    idac_di_base_damper = cfg->ffb_base_damper_fraction > 100
                            ? 100
                            : cfg->ffb_base_damper_fraction;
    
    /* Deadband is the only setting from 0.0% to 20.0%*/
    idac_di_deadband = cfg->ffb_deadband > 200
                        ? 200
                        : cfg->ffb_deadband;

    return S_OK;
}

HRESULT idac_di_dev_start(IDirectInputDevice8W* dev, HWND wnd) {
    HRESULT hr;

    assert(dev != NULL);
    assert(wnd != NULL);

    hr = IDirectInputDevice8_SetCooperativeLevel(
        dev, wnd, DISCL_BACKGROUND | DISCL_EXCLUSIVE);
    if (FAILED(hr)) {
        dprintf("DirectInput: SetCooperativeLevel failed: %08x\n", (int)hr);
        return hr;
    }

    hr = IDirectInputDevice8_SetDataFormat(dev, &c_dfDIJoystick);
    if (FAILED(hr)) {
        dprintf("DirectInput: SetDataFormat failed: %08x\n", (int)hr);
        return hr;
    }

    hr = IDirectInputDevice8_Acquire(dev);
    if (FAILED(hr)) {
        dprintf("DirectInput: Acquire failed: %08x\n", (int)hr);
        return hr;
    }

    return S_OK;
}

HRESULT idac_di_dev_poll(IDirectInputDevice8W* dev, HWND wnd,
                         union idac_di_state* out) {
    HRESULT hr;
    MSG msg;

    assert(dev != NULL);
    assert(wnd != NULL);
    assert(out != NULL);

    memset(out, 0, sizeof(*out));

    while (PeekMessageW(&msg, wnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    hr = IDirectInputDevice8_GetDeviceState(dev, sizeof(out->st), &out->st);
    if (FAILED(hr)) {
        dprintf("DirectInput: GetDeviceState error: %08x\n", (int)hr);
    }

    return hr;
}

HRESULT idac_di_ffb_init(void) {
    if (!idac_di_dev || !idac_di_wnd) {
        return E_FAIL;
    }

    return idac_di_dev_start(idac_di_dev, idac_di_wnd);
}

void idac_di_ffb_toggle(bool active) {
    if (active) {
        return;
    }

    if (idac_di_fx_constant) {
        IDirectInputEffect_Stop(idac_di_fx_constant);
        IDirectInputEffect_Release(idac_di_fx_constant);
        idac_di_fx_constant = NULL;
    }
    if (idac_di_fx_rumble) {
        IDirectInputEffect_Stop(idac_di_fx_rumble);
        IDirectInputEffect_Release(idac_di_fx_rumble);
        idac_di_fx_rumble = NULL;
    }
    if (idac_di_fx_spring) {
        IDirectInputEffect_Stop(idac_di_fx_spring);
        IDirectInputEffect_Release(idac_di_fx_spring);
        idac_di_fx_spring = NULL;
    }
    if (idac_di_fx_damper) {
        IDirectInputEffect_Stop(idac_di_fx_damper);
        IDirectInputEffect_Release(idac_di_fx_damper);
        idac_di_fx_damper = NULL;
    }
}

void idac_di_ffb_constant_force(uint8_t direction_ffb, uint8_t force) {
    uint16_t ffb_strength = idac_di_constant_force_strength * 100;
    if (ffb_strength == 0) {
        return;
    }

    DWORD axis = DIJOFS_X;
    LONG direction = 0;
    DIEFFECT fx;
    DICONSTANTFORCE cf;
    HRESULT hr;

    LONG magnitude = (LONG)(((double)force / idac_di_ffb_scale) * ffb_strength);
    cf.lMagnitude = (direction_ffb == 0) ? -magnitude : magnitude;

    memset(&fx, 0, sizeof(fx));
    fx.dwSize = sizeof(fx);
    fx.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    fx.dwDuration = INFINITE;
    fx.dwGain = DI_FFNOMINALMAX;
    fx.dwTriggerButton = DIEB_NOTRIGGER;
    fx.dwTriggerRepeatInterval = INFINITE;
    fx.cAxes = 1;
    fx.rgdwAxes = &axis;
    fx.rglDirection = &direction;
    fx.cbTypeSpecificParams = sizeof(cf);
    fx.lpvTypeSpecificParams = &cf;

    update_or_create_effect(&idac_di_fx_constant, &GUID_ConstantForce, &fx);
}

void idac_di_ffb_rumble(uint8_t force, uint8_t period) {
    uint16_t ffb_strength = idac_di_rumble_strength * 100;
    if (ffb_strength == 0) {
        return;
    }

    DWORD axis = DIJOFS_X;
    LONG direction = 0;
    DIEFFECT fx;
    DIPERIODIC pe;
    HRESULT hr;

    memset(&pe, 0, sizeof(pe));
    pe.dwMagnitude = (DWORD)(((double)force / idac_di_ffb_scale) * ffb_strength);
    pe.dwPeriod = (DWORD)force * idac_di_rumble_duration;

    memset(&fx, 0, sizeof(fx));
    fx.dwSize = sizeof(fx);
    fx.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    fx.dwDuration = INFINITE;
    fx.dwGain = DI_FFNOMINALMAX;
    fx.dwTriggerButton = DIEB_NOTRIGGER;
    fx.dwTriggerRepeatInterval = INFINITE;
    fx.cAxes = 1;
    fx.rgdwAxes = &axis;
    fx.rglDirection = &direction;
    fx.cbTypeSpecificParams = sizeof(pe);
    fx.lpvTypeSpecificParams = &pe;

    update_or_create_effect(&idac_di_fx_rumble, &GUID_Sine, &fx);
}

void idac_di_ffb_damper(uint8_t force) {
    HRESULT hr;
    uint16_t ffb_strength = idac_di_damper_strength * 100;

    if (ffb_strength == 0) {
        if (idac_di_fx_spring) {
            IDirectInputEffect_Stop(idac_di_fx_spring);
            IDirectInputEffect_Release(idac_di_fx_spring);
            idac_di_fx_spring = NULL;
        }
        if (idac_di_fx_damper) {
            IDirectInputEffect_Stop(idac_di_fx_damper);
            IDirectInputEffect_Release(idac_di_fx_damper);
            idac_di_fx_damper = NULL;
        }
        return;
    }

    DWORD axis = DIJOFS_X;
    LONG direction = 0;
    DIEFFECT fx;
    DICONDITION cond;

    /* SPRING (centering) */
    memset(&cond, 0, sizeof(cond));
    cond.lPositiveCoefficient = (LONG)(((uint32_t)force * ffb_strength) / idac_di_ffb_scale);
    cond.lNegativeCoefficient = cond.lPositiveCoefficient;
    cond.dwPositiveSaturation = DI_FFNOMINALMAX;
    cond.dwNegativeSaturation = DI_FFNOMINALMAX;
    
    /* If user enters 25, result is 0.025 * DI_FFNOMINALMAX */
    cond.lDeadBand = (DI_FFNOMINALMAX * (LONG)idac_di_deadband) / 1000;

    memset(&fx, 0, sizeof(fx));
    fx.dwSize = sizeof(fx);
    fx.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    fx.dwDuration = INFINITE;
    fx.dwGain = DI_FFNOMINALMAX;
    fx.dwTriggerButton = DIEB_NOTRIGGER;
    fx.dwTriggerRepeatInterval = INFINITE;
    fx.cAxes = 1;
    fx.rgdwAxes = &axis;
    fx.rglDirection = &direction;
    fx.cbTypeSpecificParams = sizeof(cond);
    fx.lpvTypeSpecificParams = &cond;

    update_or_create_effect(&idac_di_fx_spring, &GUID_Spring, &fx);

    /* DAMPER (resistance with baseline)  */
    memset(&cond, 0, sizeof(cond));

    LONG nominal = (LONG)(( (LONG)force * ffb_strength * idac_di_damper_spring_ratio ) / idac_di_ffb_scale);
    LONG min_baseline = (LONG)(DI_FFNOMINALMAX * (idac_di_base_damper / 100.0));

    if (nominal < min_baseline) {
        nominal = min_baseline;
    }

    cond.lPositiveCoefficient = nominal;
    cond.lNegativeCoefficient = nominal;
    cond.dwPositiveSaturation = DI_FFNOMINALMAX;
    cond.dwNegativeSaturation = DI_FFNOMINALMAX;

    memset(&fx, 0, sizeof(fx));
    fx.dwSize = sizeof(fx);
    fx.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    fx.dwDuration = INFINITE;
    fx.dwGain = DI_FFNOMINALMAX;
    fx.dwTriggerButton = DIEB_NOTRIGGER;
    fx.dwTriggerRepeatInterval = INFINITE;
    fx.cAxes = 1;
    fx.rgdwAxes = &axis;
    fx.rglDirection = &direction;
    fx.cbTypeSpecificParams = sizeof(cond);
    fx.lpvTypeSpecificParams = &cond;

    update_or_create_effect(&idac_di_fx_damper, &GUID_Damper, &fx);
}

static void update_or_create_effect(LPDIRECTINPUTEFFECT* effect_ptr, REFGUID guid, DIEFFECT* fx) {
    HRESULT hr;

    if (*effect_ptr != NULL) {
        hr = IDirectInputEffect_SetParameters(*effect_ptr, fx, DIEP_TYPESPECIFICPARAMS);
        if (SUCCEEDED(hr)) {
            return;
        } else {
            IDirectInputEffect_Stop(*effect_ptr);
            IDirectInputEffect_Release(*effect_ptr);
            *effect_ptr = NULL;
        }
    }

    IDirectInputEffect* obj = NULL;
    hr = IDirectInputDevice8_CreateEffect(idac_di_dev, guid, fx, &obj, NULL);
    if (FAILED(hr)) {
        return;
    }

    hr = IDirectInputEffect_Start(obj, fx->dwDuration, 0);
    if (FAILED(hr)) {
        IDirectInputEffect_Release(obj);
        return;
    }

    *effect_ptr = obj;
}
