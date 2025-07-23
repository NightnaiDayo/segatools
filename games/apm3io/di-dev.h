#pragma once

#include <windows.h>
#include <dinput.h>

#include <stdint.h>

union apm3_di_state {
    DIJOYSTATE st;
    uint8_t bytes[sizeof(DIJOYSTATE)];
};

HRESULT apm3_di_dev_start(IDirectInputDevice8W *dev, HWND wnd);
HRESULT apm3_di_dev_poll(
        IDirectInputDevice8W *dev,
        HWND wnd,
        union apm3_di_state *out);

