#pragma once

#include <windows.h>
#include <dinput.h>

#include <stdint.h>

#include "idzio/config.h"

union idz_di_state {
    DIJOYSTATE st;
    uint8_t bytes[sizeof(DIJOYSTATE)];
};

HRESULT idz_di_dev_init(
    const struct idz_di_config *cfg,
    IDirectInputDevice8W *dev,
    HWND wnd);

HRESULT idz_di_dev_start(IDirectInputDevice8W *dev, HWND wnd);
HRESULT idz_di_dev_poll(
        IDirectInputDevice8W *dev,
        HWND wnd,
        union idz_di_state *out);

HRESULT idz_di_ffb_init(void);
void idz_di_ffb_toggle(bool active);
void idz_di_ffb_constant_force(uint8_t direction, uint8_t force);
void idz_di_ffb_rumble(uint8_t force, uint8_t period);
void idz_di_ffb_damper(uint8_t force);
