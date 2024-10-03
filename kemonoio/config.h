#pragma once

#include <stddef.h>
#include <stdint.h>

#include <stdbool.h>

struct kemono_io_config {
    uint8_t vk_test;
    uint8_t vk_service;
    uint8_t vk_coin;

    uint8_t vk_left;
    uint8_t vk_right;
    uint8_t vk_up;
    uint8_t vk_down;
    uint8_t vk_red;
    uint8_t vk_green;
    uint8_t vk_blue;
    uint8_t vk_start;
};

void kemono_io_config_load(
        struct kemono_io_config *cfg,
        const wchar_t *filename);
