#pragma once

#include <windows.h>

#include "hkbio/hkbio.h"

struct hkb_dll {
    uint16_t api_version;
    HRESULT (*init)(void);
    HRESULT (*poll)(void);
    void (*get_opbtns)(uint8_t* opbtn);
    void (*get_gamebtns)(uint8_t* gamebtn);
    HRESULT (*led_init)(void);
    void (*led_set_leds)(uint8_t board, uint8_t* rgb);
};

struct hkb_dll_config {
    wchar_t path[MAX_PATH];
};

extern struct hkb_dll hkb_dll;

HRESULT hkb_dll_init(const struct hkb_dll_config* cfg, HINSTANCE self);
