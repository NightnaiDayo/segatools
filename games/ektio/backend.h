#pragma once

#include <stdint.h>

#include "ektio/ektio.h"

struct ekt_io_backend {
    void (*get_gamebtns)(uint32_t *gamebtn);
    void (*get_trackball)(uint8_t *x, uint8_t *y);
};
