#pragma once

#include <stdint.h>

#include "ektio/ektio.h"

struct ekt_io_backend {
    void (*get_gamebtns)(uint32_t *gamebtn);
    void (*get_trackball)(uint16_t *x, uint16_t *y);
};
