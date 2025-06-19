/*
    Serial LED implementation for chuniio
    
    Credits:
    somewhatlurker, skogaby
*/

#pragma once

#include <windows.h>

#include "mu3io/leddata.h"

HRESULT mu3_led_serial_init(wchar_t led_com[12], DWORD baud);
void mu3_led_serial_update(struct _ongeki_led_data_buf_t* data);
