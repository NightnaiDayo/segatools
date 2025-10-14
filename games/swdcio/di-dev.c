#include <windows.h>
#include <dinput.h>
#include <stdbool.h>
#include <assert.h>

#include "swdcio/di-dev.h"
#include "util/dprintf.h"

const struct swdc_di_config *swdc_di_cfg;
static HWND swdc_di_wnd;
static IDirectInputDevice8W *swdc_di_dev;

/* Individual DI Effects */
static IDirectInputEffect *swdc_di_fx = NULL;
static IDirectInputEffect *swdc_di_fx_rumble = NULL;
static IDirectInputEffect *swdc_di_fx_spring = NULL;
static IDirectInputEffect *swdc_di_fx_damper = NULL;

/* Max FFB Board value is 127 */
static const double swdc_di_ffb_scale = 127.0;

/* Helper: baseline damper fraction */
#define BASE_DAMPER() (swdc_di_cfg && swdc_di_cfg->ffb_base_damper_fraction > 0.0 ? \
                       swdc_di_cfg->ffb_base_damper_fraction : 0.20)


HRESULT swdc_di_dev_init(
    const struct swdc_di_config *cfg,
    IDirectInputDevice8W *dev,
    HWND wnd)
{
    assert(cfg);
    assert(dev);
    assert(wnd);

    swdc_di_cfg = cfg;
    swdc_di_dev = dev;
    swdc_di_wnd = wnd;

    return S_OK;
}

HRESULT swdc_di_dev_start(IDirectInputDevice8W *dev, HWND wnd)
{
    HRESULT hr;

    assert(dev);
    assert(wnd);

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

HRESULT swdc_di_dev_poll(
    IDirectInputDevice8W *dev,
    HWND wnd,
    union swdc_di_state *out)
{
    HRESULT hr;
    MSG msg;

    assert(dev);
    assert(wnd);
    assert(out);

    memset(out, 0, sizeof(*out));



    while (PeekMessageW(&msg, wnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    hr = IDirectInputDevice8_GetDeviceState(
		dev, 
		sizeof(out->st), 
		&out->st);
		
    if (FAILED(hr)) {
        dprintf("DirectInput: GetDeviceState error: %08x\n", (int)hr);
    }

    return hr;
}

HRESULT swdc_di_ffb_init(void)
{
    return swdc_di_dev_start(swdc_di_dev, swdc_di_wnd);
}

void swdc_di_ffb_toggle(bool active)
{
    if (active) return;

    if (swdc_di_fx) {
        IDirectInputEffect_Stop(swdc_di_fx);
        IDirectInputEffect_Release(swdc_di_fx);
        swdc_di_fx = NULL;
    }
    if (swdc_di_fx_rumble) {
        IDirectInputEffect_Stop(swdc_di_fx_rumble);
        IDirectInputEffect_Release(swdc_di_fx_rumble);
        swdc_di_fx_rumble = NULL;
    }
    if (swdc_di_fx_spring) {
        IDirectInputEffect_Stop(swdc_di_fx_spring);
        IDirectInputEffect_Release(swdc_di_fx_spring);
        swdc_di_fx_spring = NULL;
    }
    if (swdc_di_fx_damper) {
        IDirectInputEffect_Stop(swdc_di_fx_damper);
        IDirectInputEffect_Release(swdc_di_fx_damper);
        swdc_di_fx_damper = NULL;
    }
}

/* ------------------------------------------------------------
   Constant Force
   ------------------------------------------------------------ */
void swdc_di_ffb_constant_force(uint8_t direction_ffb, uint8_t force)
{
    uint16_t ffb_strength = swdc_di_cfg->ffb_constant_force_strength * 100;
    if (ffb_strength == 0) return;

    DWORD axis = DIJOFS_X;
    LONG direction = 0;
    DIEFFECT fx;
    DICONSTANTFORCE cf;
    HRESULT hr;

    LONG magnitude = (LONG)(((double)force / swdc_di_ffb_scale) * ffb_strength);
    cf.lMagnitude = (direction_ffb == 0) ? -magnitude : magnitude;

    memset(&fx, 0, sizeof(fx));
    fx.dwSize = sizeof(fx);
    fx.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    fx.dwDuration = INFINITE;
    fx.dwGain = DI_FFNOMINALMAX;
    fx.dwTriggerButton = DIEB_NOTRIGGER;
    fx.cAxes = 1;
    fx.rgdwAxes = &axis;
    fx.rglDirection = &direction;
    fx.cbTypeSpecificParams = sizeof(cf);
    fx.lpvTypeSpecificParams = &cf;

    if (swdc_di_fx) {
        hr = IDirectInputEffect_SetParameters(swdc_di_fx, &fx, DIEP_TYPESPECIFICPARAMS);
        if (SUCCEEDED(hr)) return;
        IDirectInputEffect_Stop(swdc_di_fx);
        IDirectInputEffect_Release(swdc_di_fx);
        swdc_di_fx = NULL;
    }

    IDirectInputEffect *obj;
    hr = IDirectInputDevice8_CreateEffect(swdc_di_dev, &GUID_ConstantForce, &fx, &obj, NULL);
    if (FAILED(hr)) {
        dprintf("DirectInput: Constant force creation failed: %08x\n", (int)hr);
        return;
    }

    hr = IDirectInputEffect_Start(obj, INFINITE, 0);
    if (FAILED(hr)) {
        dprintf("DirectInput: Constant force start failed: %08x\n", (int)hr);
        IDirectInputEffect_Release(obj);
        return;
    }

    swdc_di_fx = obj;
}

/* ------------------------------------------------------------
   Rumble
   ------------------------------------------------------------ */
void swdc_di_ffb_rumble(uint8_t force, uint8_t period)
{
    uint16_t ffb_strength = swdc_di_cfg->ffb_rumble_strength * 100;
    if (ffb_strength == 0) return;

    uint32_t ffb_duration = swdc_di_cfg->ffb_rumble_duration;

    DWORD axis = DIJOFS_X;
    LONG direction = 0;
    DIEFFECT fx;
    DIPERIODIC pe;
    HRESULT hr;

    DWORD duration = (DWORD)((double)force * ffb_duration);

    memset(&pe, 0, sizeof(pe));
    pe.dwMagnitude = (DWORD)(((double)force / swdc_di_ffb_scale) * ffb_strength);
    pe.lOffset = 0;
    pe.dwPhase = 0;
    pe.dwPeriod = duration;

    memset(&fx, 0, sizeof(fx));
    fx.dwSize = sizeof(fx);
    fx.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    fx.dwDuration = INFINITE;
    fx.dwGain = DI_FFNOMINALMAX;
    fx.dwTriggerButton = DIEB_NOTRIGGER;
    fx.cAxes = 1;
    fx.rgdwAxes = &axis;
    fx.rglDirection = &direction;
    fx.cbTypeSpecificParams = sizeof(pe);
    fx.lpvTypeSpecificParams = &pe;

    if (swdc_di_fx_rumble) {
        hr = IDirectInputEffect_SetParameters(swdc_di_fx_rumble, &fx, DIEP_TYPESPECIFICPARAMS);
        if (SUCCEEDED(hr)) return;
        IDirectInputEffect_Stop(swdc_di_fx_rumble);
        IDirectInputEffect_Release(swdc_di_fx_rumble);
        swdc_di_fx_rumble = NULL;
    }

    IDirectInputEffect *obj;
    hr = IDirectInputDevice8_CreateEffect(swdc_di_dev, &GUID_Sine, &fx, &obj, NULL);
    if (FAILED(hr)) {
        dprintf("DirectInput: Rumble creation failed: %08x\n", (int)hr);
        return;
    }

    hr = IDirectInputEffect_Start(obj, INFINITE, 0);
    if (FAILED(hr)) {
        dprintf("DirectInput: Rumble start failed: %08x\n", (int)hr);
        IDirectInputEffect_Release(obj);
        return;
    }

    swdc_di_fx_rumble = obj;
}

/* ------------------------------------------------------------
   Combined Spring + Damper (with baseline stiffness)
   ------------------------------------------------------------ */
void swdc_di_ffb_damper(uint8_t force)
{
    HRESULT hr;
    uint16_t cfg_strength = swdc_di_cfg ? (swdc_di_cfg->ffb_damper_strength * 100) : 0;
    if (cfg_strength == 0) {
        if (swdc_di_fx_spring) {
            IDirectInputEffect_Stop(swdc_di_fx_spring);
            IDirectInputEffect_Release(swdc_di_fx_spring);
            swdc_di_fx_spring = NULL;
        }
        if (swdc_di_fx_damper) {
            IDirectInputEffect_Stop(swdc_di_fx_damper);
            IDirectInputEffect_Release(swdc_di_fx_damper);
            swdc_di_fx_damper = NULL;
        }
        return;
    }

    DWORD axis = DIJOFS_X;
    LONG direction = 0;
    DIEFFECT fx;
    DICONDITION cond;

    /* ----- Spring (centering) ----- */
    memset(&cond, 0, sizeof(cond));
    cond.lOffset = 0;
    cond.lPositiveCoefficient = (LONG)(((double)force / swdc_di_ffb_scale) * cfg_strength);
    cond.lNegativeCoefficient = cond.lPositiveCoefficient;
    cond.dwPositiveSaturation = DI_FFNOMINALMAX;
    cond.dwNegativeSaturation = DI_FFNOMINALMAX;
    cond.lDeadBand = DI_FFNOMINALMAX / 50; /* 2% deadband */

    memset(&fx, 0, sizeof(fx));
    fx.dwSize = sizeof(fx);
    fx.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    fx.dwDuration = INFINITE;
    fx.dwGain = DI_FFNOMINALMAX;
    fx.dwTriggerButton = DIEB_NOTRIGGER;
    fx.cAxes = 1;
    fx.rgdwAxes = &axis;
    fx.rglDirection = &direction;
    fx.cbTypeSpecificParams = sizeof(cond);
    fx.lpvTypeSpecificParams = &cond;

    if (swdc_di_fx_spring) {
        hr = IDirectInputEffect_SetParameters(swdc_di_fx_spring, &fx, DIEP_TYPESPECIFICPARAMS);
        if (FAILED(hr)) {
            IDirectInputEffect_Stop(swdc_di_fx_spring);
            IDirectInputEffect_Release(swdc_di_fx_spring);
            swdc_di_fx_spring = NULL;
        }
    }

    if (swdc_di_fx_spring == NULL) {
        IDirectInputEffect *spring_obj = NULL;
        hr = IDirectInputDevice8_CreateEffect(swdc_di_dev, &GUID_Spring, &fx, &spring_obj, NULL);
        if (SUCCEEDED(hr)) {
            hr = IDirectInputEffect_Start(spring_obj, INFINITE, 0);
            if (SUCCEEDED(hr)) swdc_di_fx_spring = spring_obj;
            else IDirectInputEffect_Release(spring_obj);
        } else {
            dprintf("DirectInput: Create spring failed: %08x\n", (int)hr);
        }
    }

    /* ----- Damper (resistance) ----- */
    memset(&cond, 0, sizeof(cond));
    cond.lOffset = 0;

    LONG nominal = (LONG)(((double)force / swdc_di_ffb_scale) * (cfg_strength / 2));
    LONG min_baseline = (LONG)(DI_FFNOMINALMAX * BASE_DAMPER());
    if (nominal < min_baseline) nominal = min_baseline;

    cond.lPositiveCoefficient = nominal;
    cond.lNegativeCoefficient = nominal;
    cond.dwPositiveSaturation = DI_FFNOMINALMAX;
    cond.dwNegativeSaturation = DI_FFNOMINALMAX;
    cond.lDeadBand = 0;

    memset(&fx, 0, sizeof(fx));
    fx.dwSize = sizeof(fx);
    fx.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    fx.dwDuration = INFINITE;
    fx.dwGain = DI_FFNOMINALMAX;
    fx.dwTriggerButton = DIEB_NOTRIGGER;
    fx.cAxes = 1;
    fx.rgdwAxes = &axis;
    fx.rglDirection = &direction;
    fx.cbTypeSpecificParams = sizeof(cond);
    fx.lpvTypeSpecificParams = &cond;

    if (swdc_di_fx_damper) {
        hr = IDirectInputEffect_SetParameters(swdc_di_fx_damper, &fx, DIEP_TYPESPECIFICPARAMS);
        if (FAILED(hr)) {
            IDirectInputEffect_Stop(swdc_di_fx_damper);
            IDirectInputEffect_Release(swdc_di_fx_damper);
            swdc_di_fx_damper = NULL;
        }
    }

    if (swdc_di_fx_damper == NULL) {
        IDirectInputEffect *damper_obj = NULL;
        hr = IDirectInputDevice8_CreateEffect(swdc_di_dev, &GUID_Damper, &fx, &damper_obj, NULL);
        if (SUCCEEDED(hr)) {
            hr = IDirectInputEffect_Start(damper_obj, INFINITE, 0);
            if (SUCCEEDED(hr)) swdc_di_fx_damper = damper_obj;
            else IDirectInputEffect_Release(damper_obj);
        } else {
            dprintf("DirectInput: Create damper failed: %08x\n", (int)hr);
        }
    }
}
