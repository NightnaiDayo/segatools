#include <windows.h>
#include <dinput.h>

#include <assert.h>

#include "apm3io/di-dev.h"

#include "util/dprintf.h"

HRESULT apm3_di_dev_start(IDirectInputDevice8W *dev, HWND wnd)
{
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


HRESULT apm3_di_dev_poll(
        IDirectInputDevice8W *dev,
        HWND wnd,
        union apm3_di_state *out)
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
