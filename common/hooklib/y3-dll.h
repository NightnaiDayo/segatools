#pragma once

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

#include "hooklib/y3.h"


struct y3_dll {
    uint16_t api_version;
    HRESULT (*init)(void);
    HRESULT (*close)(void);
    HRESULT (*get_cards)(struct CardInfo* cards, int* len);
};

extern struct y3_dll y3_dll;

HRESULT y3_dll_init(const struct y3_dll_config *cfg, HINSTANCE self);
