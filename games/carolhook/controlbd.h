#pragma once
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

#include <stdbool.h>
#include <stdint.h>

struct controlbd_config {
    bool enable;
};

#pragma pack(push, 1)
struct controlbd_req_hdr {
    uint8_t sync;
    uint8_t dest; // unsure
    uint8_t src; // unsure
    uint8_t len; // length of the rest of the request minus checksum
    uint8_t cmd;
};

struct controlbd_req_any {
    struct controlbd_req_hdr hdr;
    uint8_t bytes[255];
};

struct controlbd_req_reset {
    struct controlbd_req_hdr hdr;
    uint8_t payload;
    uint8_t checksum;
};

struct controlbd_req_bdinfo {
    struct controlbd_req_hdr hdr;
    uint8_t checksum;
};

struct controlbd_req_fw_checksum {
    struct controlbd_req_hdr hdr;
    uint8_t checksum;
};

struct controlbd_req_timeout {
    struct controlbd_req_hdr hdr;
    uint8_t unknown;
    uint8_t checksum;
};

struct controlbd_req_polling {
    struct controlbd_req_hdr hdr;
    uint8_t unknown[20];
    uint8_t checksum;
};

struct controlbd_resp_hdr {
    uint8_t sync;
    uint8_t dest; // unsure
    uint8_t src; // unsure
    uint8_t len; // length of the rest of the request minus checksum
    uint8_t report; // 0x01 for success, anything else for failure
    uint8_t cmd;
};

struct controlbd_resp_any {
    struct controlbd_resp_hdr hdr;
    uint8_t bytes[255];
};

struct controlbd_resp_any_ack {
    struct controlbd_resp_hdr hdr;
    uint8_t ack;
    uint8_t checksum;
};

struct controlbd_resp_reset {
    struct controlbd_resp_hdr hdr;
    uint8_t checksum;
};

struct controlbd_resp_bdinfo {
    struct controlbd_resp_hdr hdr;
    uint8_t ack;
    char bd_no[9];
    char chip_no[6];
    uint8_t rev;
    uint16_t bfr_size;
    uint8_t checksum;
};

struct controlbd_resp_fw_checksum {
    struct controlbd_resp_hdr hdr;
    uint8_t ack;
    uint16_t fw_checksum;
    uint8_t checksum;
};

struct controlbd_resp_polling {
    struct controlbd_resp_hdr hdr;
    uint8_t ack;
    uint8_t unk7;
    uint8_t unk8;
    uint8_t unk9;    
    uint8_t btns_pressed;
    uint8_t blob[7];
    int8_t coord_x;
    int8_t coord_y;
    uint8_t checksum;
};

enum {
    PEN_BTN_PRESSED_BIT = 1,
    DODGE_BTN_PRESSED_BIT = 2
};

enum {
    CONTROLBD_CMD_RESET = 0x10,
    CONTROLBD_CMD_TIMEOUT = 0x11,
    CONTROLBD_CMD_RETRY = 0x12,

    CONTROLBD_CMD_GETIN = 0x20,
    CONTROLBD_CMD_GETADI = 0x21,

    CONTROLBD_CMD_SETOUTPUT = 0x30,

    CONTROLBD_CMD_INITIALIZE = 0x80,
    CONTROLBD_CMD_POLLING = 0x81,
    CONTROLBD_CMD_CUSTOM_PATTERN = 0x82,
    CONTROLBD_CMD_DEBUG_CAROL = 0x83,
    CONTROLBD_CMD_POLLING_GENERAL = 0x84,

    CONTROLBD_CMD_CMD_STATUS = 0x90,
    CONTROLBD_CMD_PORT_SETTING = 0x91,
    CONTROLBD_CMD_PWM_DUTY = 0x92,
    CONTROLBD_CMD_LED_SET = 0x93,
    CONTROLBD_CMD_LED_REFRESH = 0x94,

    CONTROLBD_CMD_DEBUG_UART = 0xB0,
    CONTROLBD_CMD_DEBUG_I2C = 0xB1,

    CONTROLBD_CMD_DEBUG_STATUS = 0xC0,

    CONTROLBD_CMD_BDINFO = 0xF0,
    CONTROLBD_CMD_FIRM_SUM = 0xF2,
    CONTROLBD_CMD_PROTOCOL = 0xF3,
    CONTROLBD_CMD_UPDATE = 0xFE,
};
#pragma pack(pop)
HRESULT controlbd_hook_init(const struct controlbd_config *cfg);