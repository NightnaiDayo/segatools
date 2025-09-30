#pragma once

#include <windows.h>

#include "jvs/jvs-bus.h"

enum {
    SEKITO_NUMPAD_R1 = 1 << 9,
    SEKITO_NUMPAD_R2 = 1 << 8,
    SEKITO_NUMPAD_R3 = 1 << 7,
    SEKITO_NUMPAD_R4 = 1 << 6,
    SEKITO_NUMPAD_C1 = 1 << 5,
    SEKITO_NUMPAD_C2 = 1 << 4,
    SEKITO_NUMPAD_C3 = 1 << 3
};

HRESULT sekito_jvs_init(struct jvs_node **root);
void sekito_jvs_set_terminal(bool is_terminal);
