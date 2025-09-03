#pragma once

#include <stddef.h>

#include "hooklib/dvd.h"
#include "hooklib/touch.h"
#include "hooklib/printer_chc.h"
#include "hooklib/printer_cx.h"

void dvd_config_load(struct dvd_config *cfg, const wchar_t *filename);
void touch_screen_config_load(struct touch_screen_config *cfg, const wchar_t *filename);
void printer_chc_config_load(struct printer_chc_config *cfg, const wchar_t *filename);
void printer_cx_config_load(struct printer_cx_config *cfg, const wchar_t *filename);
