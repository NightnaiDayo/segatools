#pragma once

#include <windows.h>

#include "kemonoio/kemonoio.h"

struct kemono_dll {
    uint16_t api_version;

    HRESULT (*init)(void);

    HRESULT (*poll)(uint16_t *ops, uint16_t *player);

    void (*jvs_read_coin_counter)(uint16_t *coins);

    HRESULT (*led_init)(void);

    void (*led_set_leds)(uint8_t board, uint8_t *rgb);

    void (*jvs_write_gpio)(uint32_t state);
};

struct kemono_dll_config {
    wchar_t path[MAX_PATH];
};

extern struct kemono_dll kemono_dll;

HRESULT kemono_dll_init(const struct kemono_dll_config *cfg, HINSTANCE self);
