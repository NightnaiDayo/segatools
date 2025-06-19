#pragma once

#include "board/led15093-frame.h"

/* Command IDs */

enum {
    LED_15093_CMD_RESET                 = 0x10,
    LED_15093_CMD_SET_TIMEOUT           = 0x11,
    LED_15093_CMD_UNK1                  = 0x12,
    LED_15093_CMD_SET_DISABLE_RESPONSE  = 0x14,
    LED_15093_CMD_SET_ID                = 0x18,
    LED_15093_CMD_CLEAR_ID              = 0x19,
    LED_15093_CMD_SET_MAX_BRIGHT        = 0x3F, // TODO
    LED_15093_CMD_UPDATE_LED            = 0x80,
    LED_15093_CMD_SET_LED               = 0x81,
    LED_15093_CMD_SET_IMM_LED           = 0x82,
    LED_15093_CMD_SET_FADE_LED          = 0x83,
    LED_15093_CMD_SET_FADE_LEVEL        = 0x84,
    LED_15093_CMD_SET_FADE_SHIFT        = 0x85,
    LED_15093_CMD_SET_AUTO_SHIFT        = 0x86,
    LED_15093_CMD_GET_BOARD_INFO        = 0xF0,
    LED_15093_CMD_GET_BOARD_STATUS      = 0xF1,
    LED_15093_CMD_GET_FW_SUM            = 0xF2,
    LED_15093_CMD_GET_PROTOCOL_VER      = 0xF3,
    LED_15093_CMD_SET_BOOTMODE          = 0xFD,
    LED_15093_CMD_FW_UPDATE             = 0xFE,
};

/* Response codes */

enum {
    LED_15093_STATUS_OK                     = 0x01,
    LED_15093_STATUS_ERR_SUM                = 0x02,
    LED_15093_STATUS_ERR_PARITY             = 0x03,
    LED_15093_STATUS_ERR_FRAMING            = 0x04,
    LED_15093_STATUS_ERR_OVERRUN            = 0x05,
    LED_15093_STATUS_ERR_BUFFER_OVERFLOW    = 0x06,
};

enum {
    LED_15093_REPORT_OK                     = 0x01,
    LED_15093_REPORT_WAIT                   = 0x02,
    LED_15093_REPORT_ERR1                   = 0x03,
    LED_15093_REPORT_ERR2                   = 0x04,
};

/* Status bitmasks */

enum {
    LED_15093_STATUS_UART_ERR_SUM               = 0x01,
    LED_15093_STATUS_UART_ERR_PARITY            = 0x02,
    LED_15093_STATUS_UART_ERR_FRAMING           = 0x04,
    LED_15093_STATUS_UART_ERR_OVERRUN           = 0x08,
    LED_15093_STATUS_UART_ERR_BUFFER_OVERFLOW   = 0x10,
};

enum {
    LED_15093_STATUS_BOARD_ERR_WDT              = 0x01,
    LED_15093_STATUS_BOARD_ERR_TIMEOUT          = 0x02,
    LED_15093_STATUS_BOARD_ERR_RESET            = 0x04,
    LED_15093_STATUS_BOARD_ERR_BOR              = 0x08,
};

enum {
    LED_15093_STATUS_CMD_ERR_BUSY               = 0x01,
    LED_15093_STATUS_CMD_ERR_UNKNOWN            = 0x02,
    LED_15093_STATUS_CMD_ERR_PARAM              = 0x04,
    LED_15093_STATUS_CMD_ERR_EXE                = 0x08,
};

/* Status types for internal use */

enum {
    LED_15093_STATUS_TYPE_BOARD     = 1,
    LED_15093_STATUS_TYPE_UART      = 2,
    LED_15093_STATUS_TYPE_CMD       = 3,
};

/* Request data structures */

struct led15093_req_reset {
    struct led15093_req_hdr hdr;
    uint8_t cmd;
    uint8_t r_type;
};

struct led15093_req_set_timeout {
    struct led15093_req_hdr hdr;
    uint8_t cmd;
    uint16_t count;
};

struct led15093_req_set_disable_response {
    struct led15093_req_hdr hdr;
    uint8_t cmd;
    bool sw;
};

struct led15093_req_set_id {
    struct led15093_req_hdr hdr;
    uint8_t cmd;
    uint8_t id;
};

struct led15093_req_set_led {
    struct led15093_req_hdr hdr;
    uint8_t cmd;
    uint8_t data[198];
};

struct led15093_req_set_fade_level {
    struct led15093_req_hdr hdr;
    uint8_t cmd;
    uint8_t depth;
    uint8_t cycle;
};

struct led15093_req_set_fade_shift {
    struct led15093_req_hdr hdr;
    uint8_t cmd;
    uint8_t target;
};

struct led15093_req_set_auto_shift {
    struct led15093_req_hdr hdr;
    uint8_t cmd;
    uint8_t count;
    uint8_t target;
};

struct led15093_req_get_board_status {
    struct led15093_req_hdr hdr;
    uint8_t cmd;
    bool clear;
};

union led15093_req_any {
    struct led15093_req_hdr hdr;
    struct led15093_req_reset reset;
    struct led15093_req_set_timeout set_timeout;
    struct led15093_req_set_disable_response set_disable_response;
    struct led15093_req_set_id set_id;
    struct led15093_req_set_led set_led;
    struct led15093_req_set_fade_level set_fade_level;
    struct led15093_req_set_fade_shift set_fade_shift;
    struct led15093_req_set_auto_shift set_auto_shift;
    struct led15093_req_get_board_status get_board_status;
    uint8_t payload[256];
};

/* Response data structures */

struct led15093_resp_any {
    struct led15093_resp_hdr hdr;
    uint8_t status;
    uint8_t cmd;
    uint8_t report;
    uint8_t data[32];
};

struct led15093_resp_timeout {
    struct led15093_resp_hdr hdr;
    uint8_t status;
    uint8_t cmd;
    uint8_t report;
    uint8_t count_upper;
    uint8_t count_lower;
};

struct led15093_resp_fw_sum {
    struct led15093_resp_hdr hdr;
    uint8_t status;
    uint8_t cmd;
    uint8_t report;
    uint8_t sum_upper;
    uint8_t sum_lower;
};

struct led15093_resp_board_info_legacy {
    struct led15093_resp_hdr hdr;
    uint8_t status;
    uint8_t cmd;
    uint8_t report;
    char board_num[8];
    uint8_t lf; // 0x0A (ASCII LF)
    char chip_num[5];
    uint8_t endcode; // Always 0xFF
    uint8_t fw_ver;
};

struct led15093_resp_board_info {
    struct led15093_resp_hdr hdr;
    uint8_t status;
    uint8_t cmd;
    uint8_t report;
    char board_num[8];
    uint8_t lf; // 0x0A (ASCII LF)
    char chip_num[5];
    uint8_t endcode; // Always 0xFF
    uint8_t fw_ver;
    uint16_t rx_buf;
};

struct led15093_resp_protocol_ver {
    struct led15093_resp_hdr hdr;
    uint8_t status;
    uint8_t cmd;
    uint8_t report;
    uint8_t mode;
    uint8_t major_ver;
    uint8_t minor_ver;
};

struct led15093_resp_set_auto_shift {
    struct led15093_resp_hdr hdr;
    uint8_t status;
    uint8_t cmd;
    uint8_t report;
    uint8_t count;
    uint8_t target;
};
