#include <windows.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "mu3io/config.h"


void mu3_io_config_load(
        struct mu3_io_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    wchar_t output_path_input[6];

    cfg->vk_test = GetPrivateProfileIntW(L"io4", L"test", VK_F1, filename);
    cfg->vk_service = GetPrivateProfileIntW(L"io4", L"service", VK_F2, filename);
    cfg->vk_coin = GetPrivateProfileIntW(L"io4", L"coin", VK_F3, filename);

    cfg->use_mouse = GetPrivateProfileIntW(L"io4", L"mouse", 0, filename);

    cfg->vk_left_1 = GetPrivateProfileIntW(L"io4", L"left1", 'A', filename);
    cfg->vk_left_2 = GetPrivateProfileIntW(L"io4", L"left2", 'S', filename);
    cfg->vk_left_3 = GetPrivateProfileIntW(L"io4", L"left3", 'D', filename);
    cfg->vk_left_side = GetPrivateProfileIntW(L"io4", L"leftSide", 'Q', filename);
    cfg->vk_right_side = GetPrivateProfileIntW(L"io4", L"rightSide", 'E', filename);
    cfg->vk_right_1 = GetPrivateProfileIntW(L"io4", L"right1", 'J', filename);
    cfg->vk_right_2 = GetPrivateProfileIntW(L"io4", L"right2", 'K', filename);
    cfg->vk_right_3 = GetPrivateProfileIntW(L"io4", L"right3", 'L', filename);
    cfg->vk_left_menu = GetPrivateProfileIntW(L"io4", L"leftMenu", 'U', filename);
    cfg->vk_right_menu = GetPrivateProfileIntW(L"io4", L"rightMenu", 'O', filename);

    cfg->cab_led_output_pipe = GetPrivateProfileIntW(L"led", L"cabLedOutputPipe", 1, filename);
    cfg->cab_led_output_serial = GetPrivateProfileIntW(L"led", L"cabLedOutputSerial", 0, filename);
    
    cfg->controller_led_output_pipe = GetPrivateProfileIntW(L"led", L"controllerLedOutputPipe", 1, filename);
    cfg->controller_led_output_serial = GetPrivateProfileIntW(L"led", L"controllerLedOutputSerial", 0, filename);

    cfg->led_serial_baud = GetPrivateProfileIntW(L"led", L"serialBaud", 921600, filename);

    GetPrivateProfileStringW(
            L"led",
            L"serialPort",
            L"COM5",
            output_path_input,
            _countof(output_path_input),
            filename);

    // Sanitize the output path. If it's a serial COM port, it needs to be prefixed
    // with `\\.\`.
    wcsncpy(cfg->led_serial_port, L"\\\\.\\", 4);
    wcsncat_s(cfg->led_serial_port, MAX_PATH, output_path_input, MAX_PATH);
}
