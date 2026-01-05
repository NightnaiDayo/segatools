#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct idz_shifter_config {
    bool auto_neutral;
};

struct idz_di_config {
    wchar_t device_name[64];
    wchar_t shifter_name[64];
    wchar_t pedals_name[64];
    wchar_t brake_axis[16];
    wchar_t accel_axis[16];
    uint8_t start;
    uint8_t view_chg;
    uint8_t up;
    uint8_t down; 
    uint8_t left;
    uint8_t right;
    uint8_t shift_dn;
    uint8_t shift_up;
    uint8_t gear[6];
    bool reverse_brake_axis;
    bool reverse_accel_axis;

    // FFB configuration
    uint8_t ffb_constant_force_strength;
    uint8_t ffb_rumble_strength;
    uint8_t ffb_damper_strength;
	uint8_t ffb_base_damper_fraction;
    uint8_t ffb_deadband;
    uint32_t ffb_rumble_duration;
};

struct idz_xi_config {
    bool single_stick_steering;
    bool linear_steering;
    uint16_t left_stick_deadzone;
    uint16_t right_stick_deadzone;
};

struct idz_io_config {
    uint8_t vk_test;
    uint8_t vk_service;
    uint8_t vk_coin;
    wchar_t mode[8];
    int restrict_;
    struct idz_shifter_config shifter;
    struct idz_di_config di;
    struct idz_xi_config xi;
};

void idz_di_config_load(struct idz_di_config *cfg, const wchar_t *filename);
void idz_xi_config_load(struct idz_xi_config *cfg, const wchar_t *filename);
void idz_io_config_load(struct idz_io_config *cfg, const wchar_t *filename);
void idz_shifter_config_load(
        struct idz_shifter_config *cfg,
        const wchar_t *filename);
