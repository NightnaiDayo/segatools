#pragma once
#include <windows.h>

#include <stdbool.h>
#include <stdint.h>

#pragma pack(push, 1)

struct touch_config {
    bool enable;
    unsigned int port_no;
    char board_id[7];
    char unit_type[9];
};

// Always starts with 0x01, always ends with 0x0D
struct touch_req {
    uint8_t sync; // Always 0x01
    uint8_t cmd[256]; // rest of the data goes here
    uint8_t tail; // Always 0x0D
    size_t data_len; // length of data
};

struct touch_report {
    uint8_t status;
    uint8_t x1;
    uint8_t x2;
    uint8_t y1;
    uint8_t y2;
    uint8_t touch_id;
};

struct touch_auto_resp {
    struct touch_report touches[10];
};

enum {
    TOUCH_MODE_STREAM    = 0x01,
    TOUCH_MODE_DOWN_UP   = 0x02,
    TOUCH_MODE_INACTIVE  = 0x03,
};

#pragma pack(pop)

HRESULT touch_hook_init(const struct touch_config *cfg);