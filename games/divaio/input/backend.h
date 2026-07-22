#pragma once

#include <stdint.h>

#include "divaio/divaio.h"

struct diva_io_backend {
    void (*get_opbtns)(uint8_t *opbtn);
    void (*get_gamebtns)(uint8_t *gamebtn);
    void (*get_slider)(uint32_t *slider);
    void (*get_auto_status)(bool* left, bool* right);
};
