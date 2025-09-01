#pragma once

#include <windows.h>

#include "board/io4.h"

enum {
    EKT_NUMPAD_SATE_R1 = 1 << 1,
    EKT_NUMPAD_SATE_R2 = 1 << 0,
    EKT_NUMPAD_SATE_R3 = 1 << 15,
    EKT_NUMPAD_SATE_R4 = 1 << 14,
    EKT_NUMPAD_SATE_C1 = 1 << 12,
    EKT_NUMPAD_SATE_C2 = 1 << 11,
    EKT_NUMPAD_SATE_C3 = 1 << 10,
    EKT_NUMPAD_TERM_R1 = 1 << 1,
    EKT_NUMPAD_TERM_R2 = 1 << 0,
    EKT_NUMPAD_TERM_R3 = 1 << 15,
    EKT_NUMPAD_TERM_R4 = 1 << 14,
    EKT_NUMPAD_TERM_C1 = 1 << 13,
    EKT_NUMPAD_TERM_C2 = 1 << 12,
    EKT_NUMPAD_TERM_C3 = 1 << 11,
};

HRESULT ekt_io4_hook_init(const struct io4_config *cfg, bool is_terminal);
