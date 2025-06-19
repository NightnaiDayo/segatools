#pragma once

#include <windows.h>
#include <dinput.h>

#include <stdint.h>

#include "swdcio/config.h"

union swdc_di_state {
    DIJOYSTATE st;
    uint8_t bytes[sizeof(DIJOYSTATE)];
};

HRESULT swdc_di_dev_init(
    const struct swdc_di_config *cfg,
    IDirectInputDevice8W *dev,
    HWND wnd);

HRESULT swdc_di_dev_start(IDirectInputDevice8W *dev, HWND wnd);
HRESULT swdc_di_dev_poll(
        IDirectInputDevice8W *dev,
        HWND wnd,
        union swdc_di_state *out);

HRESULT swdc_di_ffb_init(void);
void swdc_di_ffb_toggle(bool active);
void swdc_di_ffb_constant_force(uint8_t direction, uint8_t force);
void swdc_di_ffb_rumble(uint8_t force, uint8_t period);
void swdc_di_ffb_damper(uint8_t force);
