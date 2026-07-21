#pragma once

#include <stddef.h>
#include <stdint.h>

#include <stdbool.h>

struct hkb_kb_config {
    uint8_t vk_terminal_up;
    uint8_t vk_terminal_right;
    uint8_t vk_terminal_down;
    uint8_t vk_terminal_left;
    uint8_t vk_terminal_left_2;
    uint8_t vk_terminal_right_2;
    uint8_t vk_terminal_cancel;
    uint8_t vk_terminal_decide;
};

struct hkb_di_config {
    wchar_t device_name[64];
    uint8_t test;
    uint8_t service;
    uint8_t left_2;
    uint8_t right_2;
    uint8_t cancel;
    uint8_t decide;
    uint8_t up;
    uint8_t right;
    uint8_t down;
    uint8_t left;
};

struct hkb_io_config {
    uint8_t vk_test;
    uint8_t vk_service;
    uint8_t vk_coin;

    wchar_t mode[12];
    struct hkb_kb_config kb;
    struct hkb_di_config di;
};

void hkb_kb_config_load(struct hkb_kb_config* cfg, const wchar_t* filename);
void hkb_io_config_load(struct hkb_io_config* cfg, const wchar_t* filename);
