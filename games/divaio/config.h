#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct diva_di_config {
    wchar_t device_name[64];
    uint8_t test;
    uint8_t service;
    uint8_t start;
    uint8_t cross[2];
    uint8_t circle[2];
    uint8_t square[2];
    uint8_t triangle[2];
    uint8_t auto_left_button;
    uint8_t auto_right_button;
    wchar_t auto_left_axis[16];
    wchar_t auto_right_axis[16];
    bool auto_left_invert;
    bool auto_right_invert;
};

struct diva_xi_config {
    bool use_select_as_test;
};

struct diva_kb_config {
    uint8_t vk_cross;
    uint8_t vk_circle;
    uint8_t vk_triangle;
    uint8_t vk_square;
    uint8_t vk_start;
    uint8_t vk_slider[32];
    uint8_t vk_auto_left;
    uint8_t vk_auto_right;
};

struct diva_io_config {

    uint8_t vk_test;
    uint8_t vk_service;
    uint8_t vk_coin;

    wchar_t input_mode[16];
    wchar_t touch_mode[16];

    uint16_t hold_transfer_time_min;
    uint16_t hold_transfer_time_max;
    uint8_t dropped_input_frames;

    struct diva_kb_config kb;
    struct diva_di_config di;
    struct diva_xi_config xi;
};

void diva_io_config_load(
        struct diva_io_config *cfg,
        const wchar_t *filename);
