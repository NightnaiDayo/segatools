#pragma once

#include <stddef.h>

#include "hooklib/dvd.h"
#include "hooklib/touch.h"
#include "hooklib/printer_chc.h"
#include "hooklib/printer_cx.h"

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

void dvd_config_load(struct dvd_config *cfg, const wchar_t *filename);
void touch_screen_config_load(struct touch_screen_config *cfg, const wchar_t *filename);
void printer_chc_config_load(struct printer_chc_config *cfg, const wchar_t *filename);
void printer_cx_config_load(struct printer_cx_config *cfg, const wchar_t *filename);
void y3_config_load(struct y3_config *cfg, const wchar_t *filename);
void y3_dll_config_load(struct y3_dll_config *cfg, const wchar_t *filename);