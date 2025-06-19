#pragma once

#include <stdint.h>

#include "idacio/idacio.h"

struct idac_io_backend {
    void (*get_opbtns)(uint8_t *opbtn);
    void (*get_gamebtns)(uint8_t *gamebtn);
    void (*get_shifter)(uint8_t *gear);
    void (*get_analogs)(struct idac_io_analog_state *state);
    HRESULT (*ffb_init)(void);
    void (*ffb_toggle)(bool active);
    void (*ffb_constant_force)(uint8_t direction, uint8_t force);
    void (*ffb_rumble)(uint8_t period, uint8_t force);
    void (*ffb_damper)(uint8_t force);
};
