#include <windows.h>
#include <dinput.h>
#include <stdbool.h>
#include <assert.h>

#include "swdcio/di-dev.h"

#include "util/dprintf.h"

/* Globals */
const struct swdc_di_config *swdc_di_cfg = NULL;
static HWND swdc_di_wnd = NULL;
static IDirectInputDevice8W *swdc_di_dev = NULL;

/* Individual DI Effects */
static IDirectInputEffect *swdc_di_fx_constant = NULL;
static IDirectInputEffect *swdc_di_fx_rumble = NULL;
static IDirectInputEffect *swdc_di_fx_spring = NULL;
static IDirectInputEffect *swdc_di_fx_damper = NULL;

/* Max FFB Board value is 127 */
static const double swdc_di_ffb_scale = 127.0;

static float swdc_di_base_damper = 0.20f;  // default baseline damper

/* ------------------------------------------------------------
   Device Init / Poll
   ------------------------------------------------------------ */
HRESULT swdc_di_dev_init(
    const struct swdc_di_config *cfg,
    IDirectInputDevice8W *dev,
    HWND wnd)
{
    assert(cfg != NULL);
    assert(dev != NULL);
    assert(wnd != NULL);

    swdc_di_cfg = cfg;
    swdc_di_dev = dev;
    swdc_di_wnd = wnd;
	
	swdc_di_base_damper =
    (cfg->ffb_base_damper_fraction > 0.0f)
        ? cfg->ffb_base_damper_fraction
        : 0.20f;

    return S_OK;
}

HRESULT swdc_di_dev_start(IDirectInputDevice8W *dev, HWND wnd)
{
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

HRESULT swdc_di_dev_poll(
    IDirectInputDevice8W *dev,
    HWND wnd,
    union swdc_di_state *out)
{
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

/* ------------------------------------------------------------
   FFB init / toggle
   ------------------------------------------------------------ */
HRESULT swdc_di_ffb_init(void)
{
    if (!swdc_di_dev || !swdc_di_wnd) return E_FAIL;
    return swdc_di_dev_start(swdc_di_dev, swdc_di_wnd);
}

void swdc_di_ffb_toggle(bool active)
{
    if (active) {
		return;
	}
	
    if (swdc_di_fx_constant) {
        IDirectInputEffect_Stop(swdc_di_fx_constant);
        IDirectInputEffect_Release(swdc_di_fx_constant);
        swdc_di_fx_constant = NULL;
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
   Constant force
   ------------------------------------------------------------ */
void swdc_di_ffb_constant_force(uint8_t direction_ffb, uint8_t force)
{
    uint16_t ffb_strength = swdc_di_cfg ? (swdc_di_cfg->ffb_constant_force_strength * 100) : 0;
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
    fx.dwTriggerRepeatInterval = INFINITE;
    fx.cAxes = 1;
    fx.rgdwAxes = &axis;
    fx.rglDirection = &direction;
    fx.cbTypeSpecificParams = sizeof(cf);
    fx.lpvTypeSpecificParams = &cf;

    if (swdc_di_fx_constant) {
        hr = IDirectInputEffect_SetParameters(swdc_di_fx_constant, &fx, DIEP_TYPESPECIFICPARAMS);
        if (SUCCEEDED(hr)) return;
        IDirectInputEffect_Stop(swdc_di_fx_constant);
        IDirectInputEffect_Release(swdc_di_fx_constant);
        swdc_di_fx_constant = NULL;
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

    swdc_di_fx_constant = obj;
}

/* ------------------------------------------------------------
   Rumble
   ------------------------------------------------------------ */
void swdc_di_ffb_rumble(uint8_t force, uint8_t period)
{
    uint16_t ffb_strength = swdc_di_cfg ? (swdc_di_cfg->ffb_rumble_strength * 100) : 0;
    if (ffb_strength == 0) return;

    DWORD axis = DIJOFS_X;
    LONG direction = 0;
    DIEFFECT fx;
    DIPERIODIC pe;
    HRESULT hr;

    memset(&pe, 0, sizeof(pe));
    pe.dwMagnitude = (DWORD)(((double)force / swdc_di_ffb_scale) * ffb_strength);
    pe.lOffset = 0;
    pe.dwPhase = 0;
    pe.dwPeriod = (DWORD)((double)force * (double)(swdc_di_cfg ? swdc_di_cfg->ffb_rumble_duration : 1));

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
        dprintf("DirectInput: Rumble effect creation failed: %08x\n", (int)hr);
        return;
    }

    hr = IDirectInputEffect_Start(obj, INFINITE, 0);
    if (FAILED(hr)) {
        dprintf("DirectInput: Rumble effect start failed: %08x\n", (int)hr);
        IDirectInputEffect_Release(obj);
        return;
    }

    swdc_di_fx_rumble = obj;
}

/* ------------------------------------------------------------
   Damper: always-active spring + baseline damper
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

    /* ---------- SPRING (centering) ---------- */
    memset(&cond, 0, sizeof(cond));
    cond.lOffset = 0;
    cond.lPositiveCoefficient = (LONG)(((double)force / swdc_di_ffb_scale) * cfg_strength);
    cond.lNegativeCoefficient = cond.lPositiveCoefficient;
    cond.dwPositiveSaturation = DI_FFNOMINALMAX;
    cond.dwNegativeSaturation = DI_FFNOMINALMAX;
    LONG deadband_percent = swdc_di_cfg->ffb_deadband;
	if (deadband_percent > 100) deadband_percent = 100;  // clamp
	cond.lDeadBand = (DI_FFNOMINALMAX * deadband_percent) / 100;  // e.g. 2 → 2%



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
            dprintf("DirectInput: Create spring effect failed: %08x\n", (int)hr);
        }
    }

    /* ---------- DAMPER (resistance with baseline) ---------- */
    memset(&cond, 0, sizeof(cond));
    cond.lOffset = 0;

    LONG nominal = (LONG)(((double)force / swdc_di_ffb_scale) * (cfg_strength / 2));
    LONG min_baseline = (LONG)(DI_FFNOMINALMAX * swdc_di_base_damper);

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
    fx.dwTriggerRepeatInterval = INFINITE;
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

    hr = IDirectInputDevice8_CreateEffect(
        swdc_di_dev,
        &GUID_Damper,
        &fx,
        &damper_obj,
        NULL
    );

    if (SUCCEEDED(hr)) {
        hr = IDirectInputEffect_Start(damper_obj, INFINITE, 0);

        if (SUCCEEDED(hr)) {
            swdc_di_fx_damper = damper_obj;
        } else {
            dprintf("DirectInput: Failed to start damper effect: %08x\n", (int)hr);
            IDirectInputEffect_Release(damper_obj);
        }
    } else {
        dprintf("DirectInput: Create damper effect failed: %08x\n", (int)hr);
		}
	}

}