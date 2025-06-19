#pragma once

#include <stddef.h>
#include <stdint.h>

#include <stdbool.h>

struct mu3_io_config {
    uint8_t vk_test;
    uint8_t vk_service;
    uint8_t vk_coin;

    bool use_mouse;

    uint8_t vk_left_1;
    uint8_t vk_left_2;
    uint8_t vk_left_3;
    uint8_t vk_left_side;
    uint8_t vk_right_side;
    uint8_t vk_right_1;
    uint8_t vk_right_2;
    uint8_t vk_right_3;
    uint8_t vk_left_menu;
    uint8_t vk_right_menu;

    // Which ways to output LED information are enabled
    bool cab_led_output_pipe;
    bool cab_led_output_serial;
    
    bool controller_led_output_pipe;
    bool controller_led_output_serial;

    // The name of a COM port to output LED data on, in serial mode
    wchar_t led_serial_port[12];
    int32_t led_serial_baud;
};

void mu3_io_config_load(
        struct mu3_io_config *cfg,
        const wchar_t *filename);
