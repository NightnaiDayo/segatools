#pragma once

#include <stdint.h>

#include "idzio/idzio.h"

struct idz_io_backend {
    void (*jvs_read_buttons)(uint8_t *gamebtn);
    void (*jvs_read_shifter)(uint8_t *gear);
    void (*jvs_read_analogs)(struct idz_io_analog_state *state);
    HRESULT (*ffb_init)(void);
    void (*ffb_toggle)(bool active);
    void (*ffb_constant_force)(uint8_t direction, uint8_t force);
    void (*ffb_rumble)(uint8_t period, uint8_t force);
    void (*ffb_damper)(uint8_t force);
};
