#pragma once

#include <windows.h>

#include <stdbool.h>
#include <stdint.h>

struct led15093_config {
    bool enable;
    bool high_baudrate;
    unsigned int port_no[2];
    char board_number[8];
    char chip_number[5];
    char boot_chip_number[5];
    uint8_t fw_ver;
    uint16_t fw_sum;
};

typedef HRESULT (*io_led_init_t)(void);
typedef void (*io_led_set_leds_t)(uint8_t board, uint8_t *rgb);

HRESULT led15093_hook_init(const struct led15093_config *cfg, io_led_init_t _led_init, 
    io_led_set_leds_t _set_leds, unsigned int port_no[2]);

