#pragma once

#include <stddef.h>
#include <stdint.h>

#include <stdbool.h>

struct tokyo_kb_config {
    uint8_t vk_push_left_b;
    uint8_t vk_push_center_y;
    uint8_t vk_push_right_r;
    uint8_t vk_foot_l;
    uint8_t vk_foot_r;
    uint8_t vk_jump_1;
    uint8_t vk_jump_2;
    uint8_t vk_jump_3;
    uint8_t vk_jump_4;
    uint8_t vk_jump_5;
    uint8_t vk_jump_6;
    uint8_t vk_jump_all;
};

struct tokyo_io_config {
    uint8_t vk_test;
    uint8_t vk_service;
    uint8_t vk_coin;
    wchar_t mode[9];

    struct tokyo_kb_config kb;
};

void tokyo_io_config_load(
        struct tokyo_io_config *cfg,
        const wchar_t *filename);
