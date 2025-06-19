#pragma once

#include <stdint.h>

#include "tokyoio/tokyoio.h"

struct tokyo_io_backend {
    void (*get_gamebtns)(uint8_t *gamebtn);
    void (*get_sensors)(uint8_t *sense);
};
