#include <windows.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "divaio/config.h"

static const int diva_io_default_buttons[] = {
    VK_RIGHT, VK_DOWN, VK_LEFT, VK_UP, VK_SPACE
};

static const int diva_io_default_slider[] = {
    'Q', 'W', 'E', 'R', 'U', 'I', 'O', 'P'
};

void diva_io_config_load(
        struct diva_io_config *cfg,
        const wchar_t *filename)
{
    wchar_t key[5];
    wchar_t cell[8];
    int i;
    int c;

    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->vk_test = GetPrivateProfileIntW(L"io3", L"test", VK_F1, filename);
    cfg->vk_service = GetPrivateProfileIntW(L"io3", L"service", VK_F2, filename);
    cfg->vk_coin = GetPrivateProfileIntW(L"io3", L"coin", VK_F3, filename);

    /* Load touch input type. Mouse input by default. */
    GetPrivateProfileStringW(
            L"touch",
            L"mode",
            L"mouse",
            cfg->touch_mode,
            _countof(cfg->touch_mode),
            filename);

    for (i = 0 ; i < _countof(cfg->vk_buttons) ; i++) {
        swprintf_s(key, _countof(key), L"key%i", i + 1);
        cfg->vk_buttons[i] = GetPrivateProfileIntW(
                L"buttons",
                key,
                diva_io_default_buttons[i],
                filename);
    }

    for (c = 0 ; c < _countof(cfg->vk_slider) ; c++) {
        swprintf_s(cell, _countof(cell), L"cell%i", c + 1);
        cfg->vk_slider[c] = GetPrivateProfileIntW(
                L"slider",
                cell,
                diva_io_default_slider[c],
                filename);
    }
}
