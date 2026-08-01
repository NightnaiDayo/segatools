#pragma once

#include <windows.h>

#include "sekitoio/sekitoio.h"

struct sekito_dll {
    uint16_t api_version;
    HRESULT (*init)(void);
    HRESULT (*poll)(void);
    void (*get_opbtns)(uint8_t *opbtn);
    void (*get_gamebtns)(uint32_t *gamebtn);
    void (*get_trackball_position)(uint16_t *x, uint16_t *y);
    HRESULT (*led_init)(void);
    void (*led_set_leds)(uint8_t board, uint8_t *rgb);
};

struct sekito_dll_config {
    wchar_t path[MAX_PATH];
};

extern struct sekito_dll sekito_dll;

HRESULT sekito_dll_init(const struct sekito_dll_config *cfg, HINSTANCE self);
