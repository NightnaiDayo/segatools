#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APM3_BUTTON_COUNT 8

struct apm3_di_config {
    wchar_t device_name[64];
    uint8_t start;
    uint8_t home;
    uint8_t button[8];
};

struct apm3_xi_config {
    bool analog_stick_enabled;
};

struct apm3_kb_config {
    uint8_t vk_start;
    uint8_t vk_home;

    uint8_t vk_up;
    uint8_t vk_right;
    uint8_t vk_down;
    uint8_t vk_left;

    uint8_t vk_buttons[APM3_BUTTON_COUNT];
};

struct apm3_io_config {
    uint8_t vk_test;
    uint8_t vk_service;
    uint8_t vk_coin;

    wchar_t mode[9];

    struct apm3_kb_config kb;
    struct apm3_di_config di;
    struct apm3_xi_config xi;
};

void apm3_io_config_load(struct apm3_io_config *cfg, const wchar_t *filename);

void apm3_kb_config_load(struct apm3_kb_config *cfg, const wchar_t *filename);
void apm3_di_config_load(struct apm3_di_config *cfg, const wchar_t *filename);
void apm3_xi_config_load(struct apm3_xi_config *cfg, const wchar_t *filename);
