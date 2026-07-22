#pragma once

#include <windows.h>
#include <dinput.h>

#include <stdint.h>

union diva_di_state {
    DIJOYSTATE st;
    uint8_t bytes[sizeof(DIJOYSTATE)];
};

HRESULT diva_di_dev_start(IDirectInputDevice8W *dev, HWND wnd);
HRESULT diva_di_dev_poll(
        IDirectInputDevice8W *dev,
        HWND wnd,
        union diva_di_state *out);

