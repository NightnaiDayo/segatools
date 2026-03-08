#pragma once

#include <stddef.h>
#include <stdint.h>

#include <stdbool.h>

struct sekito_kb_config {
    uint8_t vk_menu;
    uint8_t vk_start;
    uint8_t vk_stratagem;
    uint8_t vk_stratagem_lock;
    uint8_t vk_hougu;

    uint8_t vk_tenkey_0;
    uint8_t vk_tenkey_1;
    uint8_t vk_tenkey_2;
    uint8_t vk_tenkey_3;
    uint8_t vk_tenkey_4;
    uint8_t vk_tenkey_5;
    uint8_t vk_tenkey_6;
    uint8_t vk_tenkey_7;
    uint8_t vk_tenkey_8;
    uint8_t vk_tenkey_9;
    uint8_t vk_tenkey_clear;
    uint8_t vk_tenkey_enter;

    uint8_t vk_terminal_up;
    uint8_t vk_terminal_right;
    uint8_t vk_terminal_down;
    uint8_t vk_terminal_left;
    uint8_t vk_terminal_left_2;
    uint8_t vk_terminal_right_2;
    uint8_t vk_terminal_reserve;
    uint8_t vk_terminal_cancel;
    uint8_t vk_terminal_decide;

    uint8_t x_down;
    uint8_t x_up;
    uint8_t y_down;
    uint8_t y_up;
    uint8_t speed;
};

struct sekito_io_config {
    uint8_t vk_test;
    uint8_t vk_service;
    uint8_t vk_coin;
    uint8_t vk_sw1;
    uint8_t vk_sw2;

    wchar_t mode[12];
    struct sekito_kb_config kb;
};

void sekito_kb_config_load(struct sekito_kb_config *cfg, const wchar_t *filename);
void sekito_io_config_load(
        struct sekito_io_config *cfg,
        const wchar_t *filename);
