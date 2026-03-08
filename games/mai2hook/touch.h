#pragma once

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

#include "hooklib/uart.h"

struct touch_config
{
    bool enable_1p;
    bool enable_2p;
};

enum
{
    commandRSET = 0x45,  // E
    commandHALT = 0x4C,  // L
    commandSTAT = 0x41,  // A
    commandRatio = 0x72, // r
    commandSens = 0x6B,  // k
    req_start = 0x7b,    // {
    req_end = 0x7d,      // }
    res_start = 0x28,    // (
    res_end = 0x29,      // )
};

extern const char *sensor_map[34];
const char *sensor_to_str(uint8_t sensor);

HRESULT touch_hook_init(const struct touch_config *cfg);
