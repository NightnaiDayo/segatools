#pragma once

#include "board/led15070-frame.h"

/* Command IDs */

enum {
    LED_15070_CMD_RESET                     = 0x10,
    LED_15070_CMD_SET_INPUT                 = 0x28, // No known use case
    LED_15070_CMD_SET_NORMAL_12BIT          = 0x30, // TODO
    LED_15070_CMD_SET_NORMAL_8BIT           = 0x31,
    LED_15070_CMD_SET_MULTI_FLASH_8BIT      = 0x32,
    LED_15070_CMD_SET_MULTI_FADE_8BIT       = 0x33,
    LED_15070_CMD_SET_PALETTE_7_NORMAL_LED  = 0x34, // No known use case
    LED_15070_CMD_SET_PALETTE_6_FLASH_LED   = 0x35, // No known use case
    LED_15070_CMD_SET_15DC_OUT              = 0x36, // No known use case
    LED_15070_CMD_SET_15GS_OUT              = 0x37, // No known use case
    LED_15070_CMD_SET_PSC_MAX               = 0x38, // No known use case
    LED_15070_CMD_SET_FET_OUTPUT            = 0x39,
    LED_15070_CMD_SET_GS_PALETTE            = 0x3A,
    LED_15070_CMD_DC_UPDATE                 = 0x3B,
    LED_15070_CMD_GS_UPDATE                 = 0x3C,
    LED_15070_CMD_ROTATE                    = 0x3E, // No known use case, wtf is this?
    LED_15070_CMD_SET_DC_DATA               = 0x3F,
    LED_15070_CMD_EEPROM_WRITE              = 0x7B,
    LED_15070_CMD_EEPROM_READ               = 0x7C,
    LED_15070_CMD_ACK_ON                    = 0x7D,
    LED_15070_CMD_ACK_OFF                   = 0x7E,
    LED_15070_CMD_BOARD_INFO                = 0xF0,
    LED_15070_CMD_BOARD_STATUS              = 0xF1,
    LED_15070_CMD_FW_SUM                    = 0xF2,
    LED_15070_CMD_PROTOCOL_VER              = 0xF3,
    LED_15070_CMD_TO_BOOT_MODE              = 0xFD,
    LED_15070_CMD_FW_UPDATE                 = 0xFE,
};

/* Response codes */

enum {
    LED_15070_STATUS_OK                     = 0x01,
    LED_15070_STATUS_SUM_ERR                = 0x02,
    LED_15070_STATUS_PARITY_ERR             = 0x03,
    LED_15070_STATUS_FRAMING_ERR            = 0x04,
    LED_15070_STATUS_OVERRUN_ERR            = 0x05,
    LED_15070_STATUS_BUFFER_OVERFLOW        = 0x06,
};

enum {
    LED_15070_REPORT_OK                     = 0x01,
    LED_15070_REPORT_WAIT                   = 0x02,
    LED_15070_REPORT_ERR1                   = 0x03,
    LED_15070_REPORT_ERR2                   = 0x04,
};

/* Request data structures */

struct led15070_req_any {
    struct led15070_hdr hdr;
    uint8_t cmd;
    uint8_t payload[256];
};

/* Response data structures */

struct led15070_resp_any {
    struct led15070_hdr hdr;
    uint8_t status;
    uint8_t cmd;
    uint8_t report;
    uint8_t data[32];
};

struct led15070_resp_board_info {
    struct led15070_hdr hdr;
    uint8_t status;
    uint8_t cmd;
    uint8_t report;
    char board_num[8];
    uint8_t endcode; // Always 0xFF
    uint8_t fw_ver;
};
