#pragma once

#include <stdbool.h>
#include <stdint.h>

struct led_data {
   DWORD unitCount;
   uint8_t rgba[480 * 4];
};

struct elisabeth_config {
    bool enable;
};

HRESULT elisabeth_hook_init(struct elisabeth_config *cfg);
