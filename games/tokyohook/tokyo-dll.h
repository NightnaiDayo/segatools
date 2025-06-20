#pragma once

#include <windows.h>

#include "tokyoio/tokyoio.h"

struct tokyo_dll {
    uint16_t api_version;
    HRESULT (*init)(void);
    void (*get_opbtns)(uint8_t *opbtn);
    void (*get_gamebtns)(uint8_t *gamebtn);
    void (*get_sensors)(uint8_t *sense);
    HRESULT (*gpio_out)(uint32_t state);
    HRESULT (*led_init)(void);
    void (*led_set_leds)(uint8_t board, uint8_t *rgb);
};

struct tokyo_dll_config {
    wchar_t path[MAX_PATH];
};

extern struct tokyo_dll tokyo_dll;

HRESULT tokyo_dll_init(const struct tokyo_dll_config *cfg, HINSTANCE self);
