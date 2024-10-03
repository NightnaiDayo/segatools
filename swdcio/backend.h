#pragma once

#include <stdint.h>

#include "swdcio/swdcio.h"

struct swdc_io_backend {
    void (*get_opbtns)(uint8_t *opbtn);
    void (*get_gamebtns)(uint16_t *gamebtn);
    void (*get_analogs)(struct swdc_io_analog_state *state);
    HRESULT (*ffb_init)(void);
    void (*ffb_toggle)(bool active);
    void (*ffb_constant_force)(uint8_t direction, uint8_t force);
    void (*ffb_rumble)(uint8_t period, uint8_t force);
    void (*ffb_damper)(uint8_t force);
};
