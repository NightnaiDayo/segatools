#pragma once

#include <windows.h>

#include "board/io4.h"

enum {
    SEKITO_NUMPAD_R1 = 1 << 9,
    SEKITO_NUMPAD_R2 = 1 << 8,
    SEKITO_NUMPAD_R3 = 1 << 7,
    SEKITO_NUMPAD_R4 = 1 << 6,
    SEKITO_NUMPAD_C1 = 1 << 5,
    SEKITO_NUMPAD_C2 = 1 << 4,
    SEKITO_NUMPAD_C3 = 1 << 3
};

HRESULT sekito_io4_hook_init(const struct io4_config *cfg, bool is_terminal);
