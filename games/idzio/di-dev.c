#include <windows.h>
#include <dinput.h>
#include <stdbool.h>
#include <assert.h>

#include "idzio/di-dev.h"

#include "util/dprintf.h"


const struct idz_di_config *idz_di_cfg = NULL;
static HWND idz_di_wnd = NULL;
static IDirectInputDevice8W *idz_di_dev = NULL;

/* Individual DI Effects */
static IDirectInputEffect *idz_di_fx_constant = NULL;
static IDirectInputEffect *idz_di_fx_rumble = NULL;
static IDirectInputEffect *idz_di_fx_spring = NULL;
static IDirectInputEffect *idz_di_fx_damper = NULL;

/* Max FFB Board value is 127 */
static const double idz_di_ffb_scale = 127.0;

static float idz_di_base_damper = 0.20f;  // default baseline damper


/* ------------------------------------------------------------
   Device init / poll / start
   ------------------------------------------------------------ */
HRESULT idz_di_dev_init(
    const struct idz_di_config *cfg,
    IDirectInputDevice8W *dev,
    HWND wnd)
{
    assert(cfg != NULL);
	assert(dev != NULL);
    assert(wnd != NULL);

    idz_di_cfg = cfg;
    idz_di_dev = dev;
    idz_di_wnd = wnd;
	
	idz_di_base_damper =
    (cfg->ffb_base_damper_fraction > 0.0f)
        ? cfg->ffb_base_damper_fraction
        : 0.20f;

    return S_OK;
}

HRESULT idz_di_dev_start(IDirectInputDevice8W *dev, HWND wnd)
{
    HRESULT hr;

    assert(dev != NULL);
    assert(wnd != NULL);

    hr = IDirectInputDevice8_SetCooperativeLevel(dev, wnd, DISCL_BACKGROUND | DISCL_EXCLUSIVE);
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

HRESULT idz_di_dev_poll(
    IDirectInputDevice8W *dev,
    HWND wnd,
    union idz_di_state *out)
{
    HRESULT hr;
    MSG msg;

    assert(dev != NULL);
    assert(wnd != NULL);
    assert(out != NULL);

    memset(out, 0, sizeof(*out));

    /* Pump our dummy window's message queue just in case DirectInput or an
       IHV DirectInput driver somehow relies on it */

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

/* ------------------------------------------------------------
   FFB init / toggle
   ------------------------------------------------------------ */
HRESULT idz_di_ffb_init(void)
{
    if (!idz_di_dev || !idz_di_wnd) return E_FAIL;
    return idz_di_dev_start(idz_di_dev, idz_di_wnd);
}

void idz_di_ffb_toggle(bool active)
{
    if (active) {
		return;
	}
	
    if (idz_di_fx_constant) {
        IDirectInputEffect_Stop(idz_di_fx_constant);
        IDirectInputEffect_Release(idz_di_fx_constant);
        idz_di_fx_constant = NULL;
    }
    if (idz_di_fx_rumble) {
        IDirectInputEffect_Stop(idz_di_fx_rumble);
        IDirectInputEffect_Release(idz_di_fx_rumble);
        idz_di_fx_rumble = NULL;
    }
    if (idz_di_fx_spring) {
        IDirectInputEffect_Stop(idz_di_fx_spring);
        IDirectInputEffect_Release(idz_di_fx_spring);
        idz_di_fx_spring = NULL;
    }
    if (idz_di_fx_damper) {
        IDirectInputEffect_Stop(idz_di_fx_damper);
        IDirectInputEffect_Release(idz_di_fx_damper);
        idz_di_fx_damper = NULL;
    }
}

/* ------------------------------------------------------------
   Constant force
   ------------------------------------------------------------ */
void idz_di_ffb_constant_force(uint8_t direction_ffb, uint8_t force)
{
    uint16_t ffb_strength = idz_di_cfg ? (idz_di_cfg->ffb_constant_force_strength * 100) : 0;
    if (ffb_strength == 0) return;

    DWORD axis = DIJOFS_X;
    LONG direction = 0;
    DIEFFECT fx;
    DICONSTANTFORCE cf;
    HRESULT hr;

    LONG magnitude = (LONG)(((double)force / idz_di_ffb_scale) * ffb_strength);
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

    if (idz_di_fx_constant) {
        hr = IDirectInputEffect_SetParameters(idz_di_fx_constant, &fx, DIEP_TYPESPECIFICPARAMS);
        if (SUCCEEDED(hr)) return;
        IDirectInputEffect_Stop(idz_di_fx_constant);
        IDirectInputEffect_Release(idz_di_fx_constant);
        idz_di_fx_constant = NULL;
    }

    IDirectInputEffect *obj;
    hr = IDirectInputDevice8_CreateEffect(idz_di_dev, &GUID_ConstantForce, &fx, &obj, NULL);
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

    idz_di_fx_constant = obj;
}

/* ------------------------------------------------------------
   Rumble
   ------------------------------------------------------------ */
void idz_di_ffb_rumble(uint8_t force, uint8_t period)
{
    uint16_t ffb_strength = idz_di_cfg ? (idz_di_cfg->ffb_rumble_strength * 100) : 0;
    if (ffb_strength == 0) return;

    DWORD axis = DIJOFS_X;
    LONG direction = 0;
    DIEFFECT fx;
    DIPERIODIC pe;
    HRESULT hr;

    memset(&pe, 0, sizeof(pe));
    pe.dwMagnitude = (DWORD)(((double)force / idz_di_ffb_scale) * ffb_strength);
    pe.lOffset = 0;
    pe.dwPhase = 0;
    pe.dwPeriod = (DWORD)((double)force * (double)(idz_di_cfg ? idz_di_cfg->ffb_rumble_duration : 1));

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

    if (idz_di_fx_rumble) {
        hr = IDirectInputEffect_SetParameters(idz_di_fx_rumble, &fx, DIEP_TYPESPECIFICPARAMS);
        if (SUCCEEDED(hr)) return;
        IDirectInputEffect_Stop(idz_di_fx_rumble);
        IDirectInputEffect_Release(idz_di_fx_rumble);
        idz_di_fx_rumble = NULL;
    }

    IDirectInputEffect *obj;
    hr = IDirectInputDevice8_CreateEffect(idz_di_dev, &GUID_Sine, &fx, &obj, NULL);
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

    idz_di_fx_rumble = obj;
}

/* ------------------------------------------------------------
   Damper: always-active spring + baseline damper
   ------------------------------------------------------------ */
void idz_di_ffb_damper(uint8_t force)
{
    HRESULT hr;
    uint16_t cfg_strength = idz_di_cfg ? (idz_di_cfg->ffb_damper_strength * 100) : 0;
    if (cfg_strength == 0) {
        if (idz_di_fx_spring) {
            IDirectInputEffect_Stop(idz_di_fx_spring);
            IDirectInputEffect_Release(idz_di_fx_spring);
            idz_di_fx_spring = NULL;
        }
        if (idz_di_fx_damper) {
            IDirectInputEffect_Stop(idz_di_fx_damper);
            IDirectInputEffect_Release(idz_di_fx_damper);
            idz_di_fx_damper = NULL;
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
    cond.lPositiveCoefficient = (LONG)(((double)force / idz_di_ffb_scale) * cfg_strength);
    cond.lNegativeCoefficient = cond.lPositiveCoefficient;
    cond.dwPositiveSaturation = DI_FFNOMINALMAX;
    cond.dwNegativeSaturation = DI_FFNOMINALMAX;
    LONG deadband_percent = idz_di_cfg->ffb_deadband;
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

    if (idz_di_fx_spring) {
        hr = IDirectInputEffect_SetParameters(idz_di_fx_spring, &fx, DIEP_TYPESPECIFICPARAMS);
        if (FAILED(hr)) {
            IDirectInputEffect_Stop(idz_di_fx_spring);
            IDirectInputEffect_Release(idz_di_fx_spring);
            idz_di_fx_spring = NULL;
        }
    }

    if (idz_di_fx_spring == NULL) {
        IDirectInputEffect *spring_obj = NULL;
        hr = IDirectInputDevice8_CreateEffect(idz_di_dev, &GUID_Spring, &fx, &spring_obj, NULL);
        if (SUCCEEDED(hr)) {
            hr = IDirectInputEffect_Start(spring_obj, INFINITE, 0);
            if (SUCCEEDED(hr)) idz_di_fx_spring = spring_obj;
            else IDirectInputEffect_Release(spring_obj);
        } else {
            dprintf("DirectInput: Create spring effect failed: %08x\n", (int)hr);
        }
    }

    /* ---------- DAMPER (resistance with baseline) ---------- */
    memset(&cond, 0, sizeof(cond));
    cond.lOffset = 0;

    LONG nominal = (LONG)(((double)force / idz_di_ffb_scale) * (cfg_strength / 2));
    LONG min_baseline = (LONG)(DI_FFNOMINALMAX * idz_di_base_damper);

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

    if (idz_di_fx_damper) {
        hr = IDirectInputEffect_SetParameters(idz_di_fx_damper, &fx, DIEP_TYPESPECIFICPARAMS);
        if (FAILED(hr)) {
            IDirectInputEffect_Stop(idz_di_fx_damper);
            IDirectInputEffect_Release(idz_di_fx_damper);
            idz_di_fx_damper = NULL;
        }
    }

if (idz_di_fx_damper == NULL) {
    IDirectInputEffect *damper_obj = NULL;

    hr = IDirectInputDevice8_CreateEffect(
        idz_di_dev,
        &GUID_Damper,
        &fx,
        &damper_obj,
        NULL
    );

    if (SUCCEEDED(hr)) {
        hr = IDirectInputEffect_Start(damper_obj, INFINITE, 0);

        if (SUCCEEDED(hr)) {
            idz_di_fx_damper = damper_obj;
        } else {
            dprintf("DirectInput: Failed to start damper effect: %08x\n", (int)hr);
            IDirectInputEffect_Release(damper_obj);
        }
    } else {
        dprintf("DirectInput: Create damper effect failed: %08x\n", (int)hr);
		}
	}

}
