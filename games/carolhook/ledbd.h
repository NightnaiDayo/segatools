#pragma once
#include <windows.h>

#include <stdbool.h>
#include <stdint.h>

struct ledbd_config {
    bool enable;
};

enum ledbd_cmd {
    LEDBD_CMD_UNK_10 = 0x10,
    LEDBD_CMD_UNK_7C = 0x7C,
    LEDBD_CMD_UNK_30 = 0x30,
    LEDBD_CMD_UNK_F0 = 0xF0,
};

struct ledbd_req {
    uint8_t sync; // Sync byte, always 0xE0
    uint8_t dest; // command destination id?
    uint8_t src; // command source id?
    uint8_t data_len; // length of the proceeding data bytes
    uint8_t cmd; // might be the command byte?
    uint8_t data[255]; // rest of the data, len = data_len - 1
    uint8_t checksum; // final byte is all bytes (excluding sync) added
};

HRESULT ledbd_hook_init(const struct ledbd_config *cfg);