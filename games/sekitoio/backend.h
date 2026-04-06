#pragma once

#include <stdint.h>

#include "sekitoio/sekitoio.h"

struct sekito_io_backend {
    void (*get_gamebtns)(uint32_t *gamebtn);
    void (*get_trackball)(uint16_t *x, uint16_t *y);
};
