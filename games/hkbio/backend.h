#pragma once

#include <stdint.h>

#include "hkbio/hkbio.h"

struct hkb_io_backend {
    void (*get_gamebtns)(uint16_t* gamebtn);
    void (*get_opbtns)(uint8_t* opbtn);
};
