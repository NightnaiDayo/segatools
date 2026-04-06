#pragma once

#include "board/elo-frame.h"

enum {
    ELO_CMD_TOUCH = 'T',
    ELO_CMD_QUERY_ACKNOWLEDGE = 'a',
    ELO_CMD_ACKNOWLEDGE = 'A',
    ELO_CMD_SET_RESET = 'R',
    ELO_CMD_QUERY_PARAMETER = 'p',
    ELO_CMD_SET_PARAMETER = 'P'
};

enum {
    ELO_ERR_NONE = '0',
    ELO_ERR_DIVIDE_BY_ZERO = '1',
    ELO_ERR_BAD_INPUT_PACKET = '2',
    ELO_ERR_BAD_INPUT_CHECKSUM = '3',
    ELO_ERR_INPUT_PACKET_OVERRUN = '4',
    ELO_ERR_ILLEGAL_COMMAND = '5',
    ELO_ERR_CALIBRATION_CANCELLED = '6',
    ELO_ERR_BAD_SERIAL_SETUP = '8',
    ELO_ERR_INVALID_NVRAM = '9',
    ELO_ERR_SET_UNAVAILABLE = 'A',
    ELO_ERR_UNSUPPORTED_IN_FIRM = 'B',
    ELO_ERR_ILLEGAL_SUBCOMMAND = 'C',
    ELO_ERR_OPERAND_OUT_OF_RANGE = 'D',
    ELO_ERR_INVALID_TYPE = 'E',
    ELO_ERR_FATAL_ERROR = 'F',
    ELO_ERR_QUERY_UNAVAILABLE = 'G',
    ELO_ERR_INVALID_INTERRUPT = 'H',
    ELO_ERR_NVRAM_FAILURE = 'I',
    ELO_ERR_INVALID_ADDRESS = 'J',
    ELO_ERR_FAILED_POWER_ON = 'K',
};

/* Touch report packet (IntelliTouch format) */
struct elo_packet_touch {
    struct elo_packet_hdr hdr;
    uint8_t status;   /* Touch status flags */
    uint8_t x_low;    /* X coordinate low byte */
    uint8_t x_high;   /* X coordinate high byte */
    uint8_t y_low;    /* Y coordinate low byte */
    uint8_t y_high;   /* Y coordinate high byte */
    uint8_t z_low;    /* Z coordinate (pressure) low byte */
    uint8_t z_high;   /* Z coordinate (pressure) high byte */
};

/* Acknowledge packet */
struct elo_packet_acknowledge {
    struct elo_packet_hdr hdr;
    uint8_t error_code[4];  /* Up to 4 error codes */
};

/* Reset packet */
struct elo_packet_reset {
    struct elo_packet_hdr hdr;
    uint8_t r_type;  /* '0' = hard reset, '1' = soft reset, '2' = NVRAM reset */
};

/* Parameter packet */
struct elo_packet_parameter {
    struct elo_packet_hdr hdr;
    uint8_t io;      /* I/O type: '0' = serial */
    uint8_t ser1;    /* Serial parameter 1 (baud rate, parity, etc.) */
    uint8_t ser2;    /* Serial parameter 2 (handshaking, etc.) */
};

/* Union of all packet types for easier handling */
union elo_packet_any {
    struct elo_packet_hdr hdr;
    struct elo_packet_touch touch;
    struct elo_packet_acknowledge ack;
    struct elo_packet_reset reset;
    struct elo_packet_parameter param;
    uint8_t bytes[10];
};