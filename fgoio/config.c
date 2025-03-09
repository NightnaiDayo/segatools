#include <windows.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "fgoio/config.h"


void fgo_io_config_load(
        struct fgo_io_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->vk_test = GetPrivateProfileIntW(L"io4", L"test", VK_F1, filename);
    cfg->vk_service = GetPrivateProfileIntW(L"io4", L"service", VK_F2, filename);
    cfg->vk_coin = GetPrivateProfileIntW(L"io4", L"coin", VK_F3, filename);

    cfg->input_mode = GetPrivateProfileIntW(L"io4", L"inputMode", 1, filename);

    cfg->vk_attack = GetPrivateProfileIntW(L"io4", L"attack", ' ', filename);
    cfg->vk_dash = GetPrivateProfileIntW(L"io4", L"dash", VK_LSHIFT, filename);
    cfg->vk_target = GetPrivateProfileIntW(L"io4", L"target", 'J', filename);
    cfg->vk_camera = GetPrivateProfileIntW(L"io4", L"camera", 'K', filename);
    cfg->vk_np = GetPrivateProfileIntW(L"io4", L"np", 'L', filename);

    // Standard WASD
    cfg->vk_right = GetPrivateProfileIntW(L"io4", L"right", 'D', filename);
    cfg->vk_left = GetPrivateProfileIntW(L"io4", L"left", 'A', filename);
    cfg->vk_down = GetPrivateProfileIntW(L"io4", L"down", 'S', filename);
    cfg->vk_up = GetPrivateProfileIntW(L"io4", L"up", 'W', filename);
}
