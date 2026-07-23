#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <windows.h>

struct printer_cp_config {
    bool enable;
    wchar_t printer_out_path[MAX_PATH];
    char serial_no[7];
    uint32_t print_time;
};

struct printer_cp_image {
    uint8_t* data;
    uint32_t unk1;
    uint32_t unk2;
    uint32_t unk3;
    uint32_t width;
    uint32_t height;
};

enum printer_cp_status {
    PRINTER_STATUS_READY     = 0,
    PRINTER_STATUS_PREPARING = 1,
};

enum printer_cp_error {
    PRINTER_ERROR_NONE    = 0,
    PRINTER_ERROR_GENERIC = 100,
    PRINTER_ERROR_105     = 105
};

HRESULT printer_cp_hook_init(const struct printer_cp_config* cfg, HINSTANCE self);
void printer_cp_hook_insert_hooks(HMODULE target);
