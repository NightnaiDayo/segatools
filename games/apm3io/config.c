#include <windows.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "apm3io/config.h"

const int BUTTON_DEFAULTS[] = {'Q','W','E','R','A','S','D','F'};

void apm3_io_config_load(
        struct apm3_io_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->vk_test = GetPrivateProfileIntW(L"io4", L"test", '1', filename);
    cfg->vk_service = GetPrivateProfileIntW(L"io4", L"service", '2', filename);
    cfg->vk_coin = GetPrivateProfileIntW(L"io4", L"coin", '3', filename);

    cfg->vk_start = GetPrivateProfileIntW(L"io4", L"start", 'P', filename);
    cfg->vk_home = GetPrivateProfileIntW(L"io4", L"home", 'O', filename);

    cfg->vk_up = GetPrivateProfileIntW(L"io4", L"up", VK_UP, filename);
    cfg->vk_right = GetPrivateProfileIntW(L"io4", L"right", VK_RIGHT, filename);
    cfg->vk_down = GetPrivateProfileIntW(L"io4", L"down", VK_DOWN, filename);
    cfg->vk_left = GetPrivateProfileIntW(L"io4", L"left", VK_LEFT, filename);

    wchar_t tmp[16];
    for (int i = 0; i < APM3_BUTTON_COUNT; i++) {
        swprintf_s(tmp, 32, L"button%d", i + 1);
        cfg->vk_buttons[i] = GetPrivateProfileIntW(L"io4", tmp, BUTTON_DEFAULTS[i], filename);
    }

}
