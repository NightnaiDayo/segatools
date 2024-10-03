#pragma once

#include <windows.h>

#include "swdcio/swdcio.h"

struct swdc_dll {
    uint16_t api_version;
    HRESULT (*init)(void);
    void (*get_opbtns)(uint8_t *opbtn);
    void (*get_gamebtns)(uint16_t *gamebtn);
    void (*get_analogs)(struct swdc_io_analog_state *out);
    HRESULT (*led_init)(void);
    void (*led_set_fet_output)(const uint8_t *rgb);
    void (*led_gs_update)(const uint8_t *rgb);
    void (*led_set_leds)(const uint8_t *rgb);
    HRESULT (*ffb_init)(void);
    void (*ffb_toggle)(bool active);
    void (*ffb_constant_force)(uint8_t direction, uint8_t force);
    void (*ffb_rumble)(uint8_t period, uint8_t force);
    void (*ffb_damper)(uint8_t force);
};

struct swdc_dll_config {
    wchar_t path[MAX_PATH];
};

extern struct swdc_dll swdc_dll;

HRESULT swdc_dll_init(const struct swdc_dll_config *cfg, HINSTANCE self);
