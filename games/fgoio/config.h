#pragma once

#include <stddef.h>
#include <stdint.h>

#include <stdbool.h>

struct fgo_kb_config {
    uint8_t vk_np;
    uint8_t vk_target;
    uint8_t vk_dash;
    uint8_t vk_attack;
    uint8_t vk_camera;
    uint8_t vk_right;
    uint8_t vk_left;
    uint8_t vk_down;
    uint8_t vk_up;
};

struct fgo_xi_config {
    uint16_t stick_deadzone;
};

struct fgo_io_config {
    uint8_t vk_test;
    uint8_t vk_service;
    uint8_t vk_coin;

    wchar_t mode[12];
    struct fgo_kb_config kb;
    struct fgo_xi_config xi;
};

void fgo_kb_config_load(struct fgo_kb_config *cfg, const wchar_t *filename);
void fgo_xi_config_load(struct fgo_xi_config *cfg, const wchar_t *filename);
void fgo_io_config_load(
        struct fgo_io_config *cfg,
        const wchar_t *filename);
