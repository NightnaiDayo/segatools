#pragma once

#include <windows.h>
#include <dinput.h>

#include <stdint.h>

#include "idacio/config.h"

union idac_di_state {
    DIJOYSTATE st;
    uint8_t bytes[sizeof(DIJOYSTATE)];
};

HRESULT idac_di_dev_init(
    const struct idac_di_config *cfg,
    IDirectInputDevice8W *dev,
    HWND wnd);

HRESULT idac_di_dev_start(IDirectInputDevice8W *dev, HWND wnd);
HRESULT idac_di_dev_poll(
        IDirectInputDevice8W *dev,
        HWND wnd,
        union idac_di_state *out);

HRESULT idac_di_ffb_init(void);
void idac_di_ffb_toggle(bool active);
void idac_di_ffb_constant_force(uint8_t direction, uint8_t force);
void idac_di_ffb_rumble(uint8_t force, uint8_t period);
void idac_di_ffb_damper(uint8_t force);
