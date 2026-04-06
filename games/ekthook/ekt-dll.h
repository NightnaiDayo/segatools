#pragma once

#include <windows.h>

#include "ektio/ektio.h"

struct ekt_dll {
    uint16_t api_version;
    HRESULT (*init)(void);
    HRESULT (*poll)(void);
    void (*get_opbtns)(uint8_t *opbtn);
    void (*get_gamebtns)(uint32_t *gamebtn);
    void (*get_trackball_position)(uint16_t *x, uint16_t *y);
    HRESULT (*led_init)(void);
    void (*led_set_leds)(uint8_t board, uint8_t *rgb);
};

struct ekt_dll_config {
    wchar_t path[MAX_PATH];
};

extern struct ekt_dll ekt_dll;

HRESULT ekt_dll_init(const struct ekt_dll_config *cfg, HINSTANCE self);
