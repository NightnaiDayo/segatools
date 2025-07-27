#pragma once

#include <windows.h>

#include "apm3io/apm3io.h"

struct apm3_dll {
    uint16_t api_version;
    HRESULT (*init)(void);
    HRESULT (*poll)(void);
    void (*get_opbtns)(uint8_t *opbtn);
    void (*get_gamebtns)(uint32_t *gamebtn);
    HRESULT (*led_init)(void);
    void (*led_set_leds)(uint8_t board, uint8_t *rgb);
};

struct apm3_dll_config {
    wchar_t path[MAX_PATH];
};

extern struct apm3_dll apm3_dll;

HRESULT apm3_dll_init(const struct apm3_dll_config *cfg, HINSTANCE self);
