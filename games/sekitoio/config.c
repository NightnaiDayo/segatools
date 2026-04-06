#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "sekitoio/config.h"

#include <xinput.h>


void sekito_kb_config_load(
        struct sekito_kb_config *cfg,
        const wchar_t *filename) {

    cfg->vk_menu = GetPrivateProfileIntW(L"keyboard", L"menu", 'A', filename);
    cfg->vk_start = GetPrivateProfileIntW(L"keyboard", L"start", 'S', filename);
    cfg->vk_stratagem = GetPrivateProfileIntW(L"keyboard", L"stratagem", 'D', filename);
    cfg->vk_stratagem_lock = GetPrivateProfileIntW(L"keyboard", L"stratagem_lock", 'F', filename);
    cfg->vk_hougu = GetPrivateProfileIntW(L"keyboard", L"hougu", 'G', filename);

    cfg->vk_tenkey_0 = GetPrivateProfileIntW(L"keyboard", L"tenkey_0", VK_NUMPAD0, filename);
    cfg->vk_tenkey_1 = GetPrivateProfileIntW(L"keyboard", L"tenkey_1", VK_NUMPAD1, filename);
    cfg->vk_tenkey_2 = GetPrivateProfileIntW(L"keyboard", L"tenkey_2", VK_NUMPAD2, filename);
    cfg->vk_tenkey_3 = GetPrivateProfileIntW(L"keyboard", L"tenkey_3", VK_NUMPAD3, filename);
    cfg->vk_tenkey_4 = GetPrivateProfileIntW(L"keyboard", L"tenkey_4", VK_NUMPAD4, filename);
    cfg->vk_tenkey_5 = GetPrivateProfileIntW(L"keyboard", L"tenkey_5", VK_NUMPAD5, filename);
    cfg->vk_tenkey_6 = GetPrivateProfileIntW(L"keyboard", L"tenkey_6", VK_NUMPAD6, filename);
    cfg->vk_tenkey_7 = GetPrivateProfileIntW(L"keyboard", L"tenkey_7", VK_NUMPAD7, filename);
    cfg->vk_tenkey_8 = GetPrivateProfileIntW(L"keyboard", L"tenkey_8", VK_NUMPAD8, filename);
    cfg->vk_tenkey_9 = GetPrivateProfileIntW(L"keyboard", L"tenkey_9", VK_NUMPAD9, filename);
    cfg->vk_tenkey_clear = GetPrivateProfileIntW(L"keyboard", L"tenkey_clear", VK_DECIMAL, filename);
    cfg->vk_tenkey_enter = GetPrivateProfileIntW(L"keyboard", L"tenkey_enter", VK_RETURN, filename);

    cfg->vk_terminal_decide = GetPrivateProfileIntW(L"keyboard", L"decide", 'A', filename);
    cfg->vk_terminal_cancel = GetPrivateProfileIntW(L"keyboard", L"cancel", 'S', filename);
    cfg->vk_terminal_up = GetPrivateProfileIntW(L"keyboard", L"up", VK_UP, filename);
    cfg->vk_terminal_right = GetPrivateProfileIntW(L"keyboard", L"right", VK_RIGHT, filename);
    cfg->vk_terminal_down = GetPrivateProfileIntW(L"keyboard", L"down", VK_DOWN, filename);
    cfg->vk_terminal_left = GetPrivateProfileIntW(L"keyboard", L"left", VK_LEFT, filename);
    cfg->vk_terminal_left_2 = GetPrivateProfileIntW(L"keyboard", L"left2", 'Q', filename);
    cfg->vk_terminal_right_2 = GetPrivateProfileIntW(L"keyboard", L"right2", 'W', filename);
    cfg->vk_terminal_reserve = GetPrivateProfileIntW(L"keyboard", L"reserve", 'E', filename);

    cfg->x_down = GetPrivateProfileIntW(L"keyboard", L"trackball_left", VK_LEFT, filename);
    cfg->x_up = GetPrivateProfileIntW(L"keyboard", L"trackball_right", VK_RIGHT, filename);
    cfg->y_down = GetPrivateProfileIntW(L"keyboard", L"trackball_up", VK_UP, filename);
    cfg->y_up = GetPrivateProfileIntW(L"keyboard", L"trackball_down", VK_DOWN, filename);
    cfg->speed = GetPrivateProfileIntW(L"keyboard", L"speed_modifier", 1, filename);
}

void sekito_io_config_load(
        struct sekito_io_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->vk_test = GetPrivateProfileIntW(L"io4", L"test", '1', filename);
    cfg->vk_service = GetPrivateProfileIntW(L"io4", L"service", '2', filename);
    cfg->vk_coin = GetPrivateProfileIntW(L"io4", L"coin", '3', filename);
    cfg->vk_sw1 = GetPrivateProfileIntW(L"io4", L"sw1", '4', filename);
    cfg->vk_sw2 = GetPrivateProfileIntW(L"io4", L"sw2", '5', filename);

    GetPrivateProfileStringW(
            L"io4",
            L"mode",
            L"keyboard",
            cfg->mode,
            _countof(cfg->mode),
            filename);

    sekito_kb_config_load(&cfg->kb, filename);
}
