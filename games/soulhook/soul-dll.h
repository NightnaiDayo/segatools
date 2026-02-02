#pragma once

#include <windows.h>

#include "soulio/soulio.h"

struct soul_dll {
    uint16_t api_version;
    HRESULT (*init)(void);
    HRESULT (*poll)(void);
    void (*get_opbtns)(uint8_t *opbtn);
    void (*get_gamebtns)(uint8_t *gamebtn);
    void (*get_analogs)(int16_t *stick_x, int16_t *stick_y, int16_t *view_x, int16_t *view_y);
    HRESULT (*led_init)(void);
    void (*led_set_leds)(uint8_t board, uint8_t *rgb);
};

struct soul_dll_config {
    wchar_t path[MAX_PATH];
};

extern struct soul_dll soul_dll;

HRESULT soul_dll_init(const struct soul_dll_config *cfg, HINSTANCE self);
