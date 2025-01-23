#pragma once

#include <windows.h>

#include "mai2io/mai2io.h"

struct mai2_dll {
    uint16_t api_version;
    HRESULT (*init)(void);
    HRESULT (*poll)(void);
    void (*get_opbtns)(uint8_t *opbtn);
    void (*get_gamebtns)(uint16_t *player1, uint16_t *player2);
    HRESULT (*touch_init)(void);
    void (*touch_set_sens)(uint8_t *bytes);
    void (*touch_1p_start)(mai2_io_touch_callback_t callback);
    void (*touch_1p_stop)(void);
    void (*touch_2p_start)(mai2_io_touch_callback_t callback);
    void (*touch_2p_stop)(void);
    HRESULT (*led_init)(void);
    void (*led_set_fet_output)(const uint8_t *rgb);
    void (*led_dc_update)(const uint8_t *rgb);
    void (*led_gs_update)(const uint8_t *rgb);
};

struct mai2_dll_config {
    wchar_t path[MAX_PATH];
};

extern struct mai2_dll mai2_dll;

HRESULT mai2_dll_init(const struct mai2_dll_config *cfg, HINSTANCE self);
