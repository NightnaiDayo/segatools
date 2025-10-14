#include <windows.h>
#include <dinput.h>
#include <stdbool.h>
#include <assert.h>

#include "idacio/di-dev.h"

#include "util/dprintf.h"

/* Globals */
const struct idac_di_config *idac_di_cfg = NULL;
static HWND idac_di_wnd = NULL;
static IDirectInputDevice8W *idac_di_dev = NULL;

/* Effect handles */
static IDirectInputEffect *idac_di_fx_constant = NULL;
static IDirectInputEffect *idac_di_fx_rumble = NULL;
static IDirectInputEffect *idac_di_fx_spring = NULL;
static IDirectInputEffect *idac_di_fx_damper = NULL;

/* FFB board scale */
static const double idac_di_ffb_scale = 127.0;

/* Macro: get baseline damper fraction from config (default medium = 0.28) */
#define BASE_DAMPER() (idac_di_cfg && idac_di_cfg->ffb_base_damper_fraction > 0.0 ? \
                       idac_di_cfg->ffb_base_damper_fraction : 0.20)

/* ------------------------------------------------------------
   Device init / poll / start
   ------------------------------------------------------------ */
HRESULT idac_di_dev_init(
    const struct idac_di_config *cfg,
    IDirectInputDevice8W *dev,
    HWND wnd)
{
    assert(cfg != NULL);
    assert(dev != NULL);
    assert(wnd != NULL);

    idac_di_cfg = cfg;
    idac_di_dev = dev;
    idac_di_wnd = wnd;

    return S_OK;
}

HRESULT idac_di_dev_start(IDirectInputDevice8W *dev, HWND wnd)
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

HRESULT idac_di_dev_poll(
    IDirectInputDevice8W *dev,
    HWND wnd,
    union idac_di_state *out)
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
HRESULT idac_di_ffb_init(void)
{
    if (!idac_di_dev || !idac_di_wnd) return E_FAIL;
    return idac_di_dev_start(idac_di_dev, idac_di_wnd);
}

void idac_di_ffb_toggle(bool active)
{
    if (active) return;

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

/* ------------------------------------------------------------
   Constant force
   ------------------------------------------------------------ */
void idac_di_ffb_constant_force(uint8_t direction_ffb, uint8_t force)
{
    uint16_t ffb_strength = idac_di_cfg ? (idac_di_cfg->ffb_constant_force_strength * 100) : 0;
    if (ffb_strength == 0) return;

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

    if (idac_di_fx_constant) {
        hr = IDirectInputEffect_SetParameters(idac_di_fx_constant, &fx, DIEP_TYPESPECIFICPARAMS);
        if (SUCCEEDED(hr)) return;
        IDirectInputEffect_Stop(idac_di_fx_constant);
        IDirectInputEffect_Release(idac_di_fx_constant);
        idac_di_fx_constant = NULL;
    }

    IDirectInputEffect *obj;
    hr = IDirectInputDevice8_CreateEffect(idac_di_dev, &GUID_ConstantForce, &fx, &obj, NULL);
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

    idac_di_fx_constant = obj;
}

/* ------------------------------------------------------------
   Rumble
   ------------------------------------------------------------ */
void idac_di_ffb_rumble(uint8_t force, uint8_t period)
{
    uint16_t ffb_strength = idac_di_cfg ? (idac_di_cfg->ffb_rumble_strength * 100) : 0;
    if (ffb_strength == 0) return;

    DWORD axis = DIJOFS_X;
    LONG direction = 0;
    DIEFFECT fx;
    DIPERIODIC pe;
    HRESULT hr;

    memset(&pe, 0, sizeof(pe));
    pe.dwMagnitude = (DWORD)(((double)force / idac_di_ffb_scale) * ffb_strength);
    pe.lOffset = 0;
    pe.dwPhase = 0;
    pe.dwPeriod = (DWORD)((double)force * (double)(idac_di_cfg ? idac_di_cfg->ffb_rumble_duration : 1));

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

    if (idac_di_fx_rumble) {
        hr = IDirectInputEffect_SetParameters(idac_di_fx_rumble, &fx, DIEP_TYPESPECIFICPARAMS);
        if (SUCCEEDED(hr)) return;
        IDirectInputEffect_Stop(idac_di_fx_rumble);
        IDirectInputEffect_Release(idac_di_fx_rumble);
        idac_di_fx_rumble = NULL;
    }

    IDirectInputEffect *obj;
    hr = IDirectInputDevice8_CreateEffect(idac_di_dev, &GUID_Sine, &fx, &obj, NULL);
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

    idac_di_fx_rumble = obj;
}

/* ------------------------------------------------------------
   Damper: always-active spring + baseline damper
   ------------------------------------------------------------ */
void idac_di_ffb_damper(uint8_t force)
{
    HRESULT hr;
    uint16_t cfg_strength = idac_di_cfg ? (idac_di_cfg->ffb_damper_strength * 100) : 0;
    if (cfg_strength == 0) {
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

    /* ---------- SPRING (centering) ---------- */
    memset(&cond, 0, sizeof(cond));
    cond.lOffset = 0;
    cond.lPositiveCoefficient = (LONG)(((double)force / idac_di_ffb_scale) * cfg_strength);
    cond.lNegativeCoefficient = cond.lPositiveCoefficient;
    cond.dwPositiveSaturation = DI_FFNOMINALMAX;
    cond.dwNegativeSaturation = DI_FFNOMINALMAX;
    cond.lDeadBand = DI_FFNOMINALMAX / 50; /* small deadband ~2% */

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

    if (idac_di_fx_spring) {
        hr = IDirectInputEffect_SetParameters(idac_di_fx_spring, &fx, DIEP_TYPESPECIFICPARAMS);
        if (FAILED(hr)) {
            IDirectInputEffect_Stop(idac_di_fx_spring);
            IDirectInputEffect_Release(idac_di_fx_spring);
            idac_di_fx_spring = NULL;
        }
    }

    if (idac_di_fx_spring == NULL) {
        IDirectInputEffect *spring_obj = NULL;
        hr = IDirectInputDevice8_CreateEffect(idac_di_dev, &GUID_Spring, &fx, &spring_obj, NULL);
        if (SUCCEEDED(hr)) {
            hr = IDirectInputEffect_Start(spring_obj, INFINITE, 0);
            if (SUCCEEDED(hr)) idac_di_fx_spring = spring_obj;
            else IDirectInputEffect_Release(spring_obj);
        } else {
            dprintf("DirectInput: Create spring effect failed: %08x\n", (int)hr);
        }
    }

    /* ---------- DAMPER (resistance with baseline) ---------- */
    memset(&cond, 0, sizeof(cond));
    cond.lOffset = 0;

    LONG nominal = (LONG)(((double)force / idac_di_ffb_scale) * (cfg_strength / 2));
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
    fx.dwTriggerRepeatInterval = INFINITE;
    fx.cAxes = 1;
    fx.rgdwAxes = &axis;
    fx.rglDirection = &direction;
    fx.cbTypeSpecificParams = sizeof(cond);
    fx.lpvTypeSpecificParams = &cond;

    if (idac_di_fx_damper) {
        hr = IDirectInputEffect_SetParameters(idac_di_fx_damper, &fx, DIEP_TYPESPECIFICPARAMS);
        if (FAILED(hr)) {
            IDirectInputEffect_Stop(idac_di_fx_damper);
            IDirectInputEffect_Release(idac_di_fx_damper);
            idac_di_fx_damper = NULL;
        }
    }

    if (idac_di_fx_damper == NULL) {
        IDirectInputEffect *damper_obj = NULL;
        hr = IDirectInputDevice8_CreateEffect(idac_di_dev, &GUID_Damper, &fx, &damper_obj, NULL);
        if (SUCCEEDED(hr)) {
            hr = IDirectInputEffect_Start(damper_obj, INFINITE, 0);
            if (SUCCEEDED(hr)) idac_di_fx_damper = damper_obj;
            else IDirectInputEffect_Release(damper_obj);
        } else {
            dprintf("DirectInput: Create damper effect failed: %08x\n", (int)hr);
        }
    }
}
