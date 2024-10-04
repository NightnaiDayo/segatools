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
static IDirectInputEffect *swdc_di_fx;
static IDirectInputEffect *swdc_di_fx_rumble;
static IDirectInputEffect *swdc_di_fx_damper;

/* Max FFB Board value is 127 */
static const double swdc_di_ffb_scale = 127.0;

HRESULT swdc_di_dev_init(
    const struct swdc_di_config *cfg,
    IDirectInputDevice8W *dev,
    HWND wnd)
{
    HRESULT hr;

    assert(dev != NULL);
    assert(wnd != NULL);

    swdc_di_cfg = cfg;
    swdc_di_dev = dev;
    swdc_di_wnd = wnd;

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
        dprintf("DirectInput: GetDeviceState error: %08x\n", (int) hr);
    }

    /* JVS lacks a protocol for reporting hardware errors from poll command
       responses, so this ends up returning zeroed input state instead. */

    return hr;
}

HRESULT swdc_di_dev_start(IDirectInputDevice8W *dev, HWND wnd) {
    HRESULT hr;

    assert(dev != NULL);
    assert(wnd != NULL);

    hr = IDirectInputDevice8_SetCooperativeLevel(
            dev,
            wnd,
            DISCL_BACKGROUND | DISCL_EXCLUSIVE);

    if (FAILED(hr)) {
        dprintf("DirectInput: SetCooperativeLevel failed: %08x\n", (int) hr);

        return hr;
    }

    hr = IDirectInputDevice8_SetDataFormat(dev, &c_dfDIJoystick);

    if (FAILED(hr)) {
        dprintf("DirectInput: SetDataFormat failed: %08x\n", (int) hr);

        return hr;
    }

    hr = IDirectInputDevice8_Acquire(dev);

    if (FAILED(hr)) {
        dprintf("DirectInput: Acquire failed: %08x\n", (int) hr);

        return hr;
    }

    return hr;
}

HRESULT swdc_di_ffb_init(void)
{
    HRESULT hr;

    hr = swdc_di_dev_start(swdc_di_dev, swdc_di_wnd);

    if (FAILED(hr)) {
        return hr;
    }

    return S_OK;
}

void swdc_di_ffb_toggle(bool active)
{
    if (active) {
        return;
    }

    /* Stop and release all effects */
    /* I never programmed DirectInput Effects, so this might be bad practice. */
    if (swdc_di_fx != NULL) {
        IDirectInputEffect_Stop(swdc_di_fx);
        IDirectInputEffect_Release(swdc_di_fx);
        swdc_di_fx = NULL;
    }

    if (swdc_di_fx_rumble != NULL) {
        IDirectInputEffect_Stop(swdc_di_fx_rumble);
        IDirectInputEffect_Release(swdc_di_fx_rumble);
        swdc_di_fx_rumble = NULL;
    }

    if (swdc_di_fx_damper != NULL) {
        IDirectInputEffect_Stop(swdc_di_fx_damper);
        IDirectInputEffect_Release(swdc_di_fx_damper);
        swdc_di_fx_damper = NULL;
    }
}

void swdc_di_ffb_constant_force(uint8_t direction_ffb, uint8_t force)
{
    /* DI expects a magnitude in the range of -10.000 to 10.000 */
    uint16_t ffb_strength = swdc_di_cfg->ffb_constant_force_strength * 100;
    if (ffb_strength == 0) {
        return;
    }

    DWORD axis;
    LONG direction;
    DIEFFECT fx;
    DICONSTANTFORCE cf;
    HRESULT hr;

    /* Direction 0: move to the right, 1: move to the left */
    LONG magnitude = (LONG)(((double)force / swdc_di_ffb_scale) * ffb_strength);
    cf.lMagnitude = (direction_ffb == 0) ? -magnitude : magnitude;

    axis = DIJOFS_X;
    /* Irrelevant as magnitude descripbes the direction */
    direction = 0;

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

    if (swdc_di_fx != NULL) {
        // Try to update the existing effect
        hr = IDirectInputEffect_SetParameters(swdc_di_fx, &fx, DIEP_TYPESPECIFICPARAMS);
        
        if (SUCCEEDED(hr)) {
            return;
        } else {
            dprintf("DirectInput: Failed to update constant force feedback, recreating effect: %08x\n", (int)hr);
            // Stop and release the current effect if updating fails
            IDirectInputEffect_Stop(swdc_di_fx);
            IDirectInputEffect_Release(swdc_di_fx);
            swdc_di_fx = NULL;
        }
    }

    // Create a new constant force effect
    IDirectInputEffect *obj;
    hr = IDirectInputDevice8_CreateEffect(
            swdc_di_dev,
            &GUID_ConstantForce,
            &fx,
            &obj,
            NULL);

    if (FAILED(hr)) {
        dprintf("DirectInput: Constant force feedback creation failed: %08x\n", (int) hr);
        return;
    }

    hr = IDirectInputEffect_Start(obj, INFINITE, 0);
    if (FAILED(hr)) {
        dprintf("DirectInput: Constant force feedback start failed: %08x\n", (int) hr);
        IDirectInputEffect_Release(obj);
        return;
    }

    swdc_di_fx = obj;
}

void swdc_di_ffb_rumble(uint8_t force, uint8_t period)
{
    /* DI expects a magnitude in the range of -10.000 to 10.000 */
    uint16_t ffb_strength = swdc_di_cfg->ffb_rumble_strength * 100;
    if (ffb_strength == 0) {
        return;
    }

    uint32_t ffb_duration = swdc_di_cfg->ffb_rumble_duration;

    DWORD axis;
    LONG direction;
    DIEFFECT fx;
    DIPERIODIC pe;
    HRESULT hr;

    /* Duration in microseconds,
       Might be totally wrong as especially on FANATEC wheels as this code will
       crash the game. TODO: Figure out why this effect will crash on FANATEC! */
    DWORD duration = (DWORD)((double)force * ffb_duration);

    memset(&pe, 0, sizeof(pe));
    pe.dwMagnitude = (DWORD)(((double)force / swdc_di_ffb_scale) * ffb_strength);
    pe.lOffset = 0;
    pe.dwPhase = 0;
    pe.dwPeriod = duration;

    axis = DIJOFS_X;
    direction = 0;

    memset(&fx, 0, sizeof(fx));
    fx.dwSize = sizeof(fx);
    fx.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    fx.dwDuration = duration;
    fx.dwGain = DI_FFNOMINALMAX;
    fx.dwTriggerButton = DIEB_NOTRIGGER;
    fx.dwTriggerRepeatInterval = INFINITE;
    fx.cAxes = 1;
    fx.rgdwAxes = &axis;
    fx.rglDirection = &direction;
    fx.cbTypeSpecificParams = sizeof(pe);
    fx.lpvTypeSpecificParams = &pe;

    if (swdc_di_fx_rumble != NULL) {
        // Try to update the existing effect
        hr = IDirectInputEffect_SetParameters(swdc_di_fx_rumble, &fx, DIEP_TYPESPECIFICPARAMS);
        
        if (SUCCEEDED(hr)) {
            return;
        } else {
            dprintf("DirectInput: Failed to update periodic force feedback, recreating effect: %08x\n", (int)hr);
            // Stop and release the current effect if updating fails
            IDirectInputEffect_Stop(swdc_di_fx_rumble);
            IDirectInputEffect_Release(swdc_di_fx_rumble);
            swdc_di_fx_rumble = NULL;
        }
    }

    IDirectInputEffect *obj;
    hr = IDirectInputDevice8_CreateEffect(
            swdc_di_dev,
            &GUID_Sine,
            &fx,
            &obj,
            NULL);

    if (FAILED(hr)) {
        dprintf("DirectInput: Periodic force feedback creation failed: %08x\n", (int) hr);
        return;
    }

    hr = IDirectInputEffect_Start(obj, INFINITE, 0);
    if (FAILED(hr)) {
        dprintf("DirectInput: Periodic force feedback start failed: %08x\n", (int) hr);
        IDirectInputEffect_Release(obj);
        return;
    }

    swdc_di_fx_rumble = obj;
}

void swdc_di_ffb_damper(uint8_t force)
{
    /* DI expects a coefficient in the range of -10.000 to 10.000 */
    uint16_t ffb_strength = swdc_di_cfg->ffb_damper_strength * 100;
    if (ffb_strength == 0) {
        return;
    }

    DWORD axis;
    LONG direction;
    DIEFFECT fx;
    DICONDITION cond;
    HRESULT hr;

    memset(&cond, 0, sizeof(cond));
    cond.lOffset = 0;
    cond.lPositiveCoefficient = (LONG)(((double)force / swdc_di_ffb_scale) * ffb_strength);
    cond.lNegativeCoefficient = (LONG)(((double)force / swdc_di_ffb_scale) * ffb_strength);
    /* Not sure on this one */
    cond.dwPositiveSaturation = DI_FFNOMINALMAX;
    cond.dwNegativeSaturation = DI_FFNOMINALMAX;
    cond.lDeadBand = 0;

    axis = DIJOFS_X;
    direction = 0;

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

    if (swdc_di_fx_damper != NULL) {
        // Try to update the existing effect
        hr = IDirectInputEffect_SetParameters(swdc_di_fx_damper, &fx, DIEP_TYPESPECIFICPARAMS);
        
        if (SUCCEEDED(hr)) {
            return;
        } else {
            dprintf("DirectInput: Failed to update damper force feedback, recreating effect: %08x\n", (int)hr);
            // Stop and release the current effect if updating fails
            IDirectInputEffect_Stop(swdc_di_fx_damper);
            IDirectInputEffect_Release(swdc_di_fx_damper);
            swdc_di_fx_damper = NULL;
        }
    }

    // Create a new damper force effect
    IDirectInputEffect *obj;
    hr = IDirectInputDevice8_CreateEffect(
            swdc_di_dev,
            &GUID_Damper,
            &fx,
            &obj,
            NULL);

    if (FAILED(hr)) {
        dprintf("DirectInput: Damper force feedback creation failed: %08x\n", (int) hr);
        return;
    }

    hr = IDirectInputEffect_Start(obj, INFINITE, 0);
    if (FAILED(hr)) {
        dprintf("DirectInput: Damper force feedback start failed: %08x\n", (int) hr);
        IDirectInputEffect_Release(obj);
        return;
    }

    swdc_di_fx_damper = obj;
}
