#pragma once

#include <stdint.h>

#include "apm3io/apm3io.h"

struct apm3_io_backend {
    void (*get_opbtns)(uint8_t *opbtn);
    void (*get_gamebtns)(uint16_t *gamebtn);
};
