#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

struct y3_config {
    bool enable;

    float dll_version;
    float firm_version;
    char firm_name_field[5];
    char firm_name_printer[5];
    char target_code_field[5];
    char target_code_printer[5];
    uint8_t port_field;
    uint8_t port_printer;
};

struct y3_dll_config {
    wchar_t path[MAX_PATH];
};

void y3_config_load(
        struct y3_config *cfg,
        const wchar_t *filename);

void y3_dll_config_load(
        struct y3_dll_config *cfg,
        const wchar_t *filename);