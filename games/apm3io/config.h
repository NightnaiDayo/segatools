#pragma once

#include <stddef.h>
#include <stdint.h>

#include <stdbool.h>

#define APM3_BUTTON_COUNT 8

struct apm3_io_config {
    uint8_t vk_test;
    uint8_t vk_service;
    uint8_t vk_coin;

    uint8_t vk_start;
    uint8_t vk_home;

    uint8_t vk_up;
    uint8_t vk_right;
    uint8_t vk_down;
    uint8_t vk_left;

    uint8_t vk_buttons[APM3_BUTTON_COUNT];
};

void apm3_io_config_load(
        struct apm3_io_config *cfg,
        const wchar_t *filename);
