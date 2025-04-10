#pragma once

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

struct printer_config {
    bool enable;
    bool rotate_180;
    char serial_no[8];
    wchar_t main_fw_path[MAX_PATH];
    wchar_t dsp_fw_path[MAX_PATH];
    wchar_t param_fw_path[MAX_PATH];
    wchar_t printer_out_path[MAX_PATH];
    uint32_t wait_time;
};

void printer_hook_init(const struct printer_config *cfg, int rfid_port_no, HINSTANCE self);
void printer_hook_insert_hooks(HMODULE target);

void printer_set_dimensions(int width, int height);
int WINAPI fwdlusb_updateFirmware_main(uint8_t update, LPCSTR filename, uint16_t *rResult);
int WINAPI fwdlusb_updateFirmware_dsp(uint8_t update, LPCSTR filename, uint16_t *rResult);
int WINAPI fwdlusb_updateFirmware_param(uint8_t update, LPCSTR filename, uint16_t *rResult);