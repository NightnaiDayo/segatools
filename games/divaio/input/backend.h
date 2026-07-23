#pragma once

#include <stdint.h>

#include "divaio/divaio.h"

struct diva_button_state {
    bool primary;
    bool prevPrimary;
    bool secondary;
    bool prevSecondary;
    uint8_t dropFrames;
    ULONGLONG doubleTapTimeoutBegin;
    ULONGLONG doubleTapTimeoutEnd;
};

struct diva_button_states {
    bool start;
    struct diva_button_state triangle;
    struct diva_button_state square;
    struct diva_button_state circle;
    struct diva_button_state cross;
};

struct diva_io_backend {
    void (*get_opbtns)(uint8_t *opbtn);
    void (*get_gamebtns)(struct diva_button_states *gamebtn);
    void (*get_slider)(uint32_t *slider);
    void (*get_auto_status)(bool* left, bool* right);
};
