#pragma once

#include <windows.h>

#include "fgoio/fgoio.h"

struct fgo_dll {
    uint16_t api_version;
    HRESULT (*init)(void);
    HRESULT (*poll)(void);
    void (*get_opbtns)(uint8_t *opbtn);
    void (*get_gamebtns)(uint8_t *gamebtn);
    void (*get_analogs)(int16_t *stick_x, int16_t *stick_y);
    HRESULT (*led_init)(void);
    void (*led_set_leds)(uint8_t board, uint8_t *rgb);
};

struct fgo_dll_config {
    wchar_t path[MAX_PATH];
};

extern struct fgo_dll fgo_dll;

HRESULT fgo_dll_init(const struct fgo_dll_config *cfg, HINSTANCE self);
