#pragma once

#include <stdint.h>

#include "fgoio/fgoio.h"

struct fgo_io_backend {
    void (*get_gamebtns)(uint8_t *gamebtn);
    void (*get_analogs)(int16_t *x, int16_t *y);
};
