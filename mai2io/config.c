#include <windows.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "mai2io/config.h"

/*
Maimai DX Default key binding
1P: self-explanatory
2P: (Numpad) 8, 9, 6, 3, 2, 1, 4, 7, *
*/
static const int mai2_io_1p_default[] = {'W', 'E', 'D', 'C', 'X', 'Z', 'A', 'Q', '3'};
static const int mai2_io_2p_default[] = {0x68, 0x69, 0x66, 0x63, 0x62, 0x61, 0x64, 0x67, 0x54};

void mai2_io_config_load(
        struct mai2_io_config *cfg,
        const wchar_t *filename)
{
    wchar_t key[16];
    int i;

    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->vk_test = GetPrivateProfileIntW(L"io4", L"test", VK_F1, filename);
    cfg->vk_service = GetPrivateProfileIntW(L"io4", L"service", VK_F2, filename);
    cfg->vk_coin = GetPrivateProfileIntW(L"io4", L"coin", VK_F3, filename);
    cfg->vk_btn_enable = GetPrivateProfileIntW(L"button", L"enable", 1, filename);
    cfg->debug_input_1p = GetPrivateProfileIntW(L"touch", L"debug_input_1p", 0, filename);
    cfg->debug_input_2p = GetPrivateProfileIntW(L"touch", L"debug_input_2p", 0, filename);

    for (i = 0 ; i < 9 ; i++) {
        swprintf_s(key, _countof(key), L"1p_btn%i", i + 1);
        cfg->vk_1p_btn[i] = GetPrivateProfileIntW(
                L"button",
                key,
                mai2_io_1p_default[i],
                filename);
    }

    for (i = 0 ; i < 9 ; i++) {
        swprintf_s(key, _countof(key), L"2p_btn%i", i + 1);
        cfg->vk_2p_btn[i] = GetPrivateProfileIntW(
                L"button",
                key,
                mai2_io_2p_default[i],
                filename);
    }
}
