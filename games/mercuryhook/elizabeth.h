#pragma once

#include <stdbool.h>
#include <stdint.h>

struct led_data {
   DWORD unitCount;
   uint8_t rgba[480 * 4];
};

struct elizabeth_config {
    bool enable;
};

HRESULT elizabeth_hook_init(struct elizabeth_config *cfg);
