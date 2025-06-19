#include <windows.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "kemonoio/config.h"

void kemono_io_config_load(
        struct kemono_io_config *cfg,
        const wchar_t *filename) {

    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->vk_test = GetPrivateProfileIntW(L"io3", L"test", '1', filename);
    cfg->vk_service = GetPrivateProfileIntW(L"io3", L"service", '2', filename);
    cfg->vk_coin = GetPrivateProfileIntW(L"io3", L"coin", '3', filename);

    cfg->vk_left = GetPrivateProfileIntW(L"io3", L"left", VK_LEFT, filename);
    cfg->vk_right = GetPrivateProfileIntW(L"io3", L"right", VK_RIGHT, filename);
    cfg->vk_up = GetPrivateProfileIntW(L"io3", L"up", VK_UP, filename);
    cfg->vk_down = GetPrivateProfileIntW(L"io3", L"down", VK_DOWN, filename);
    cfg->vk_red = GetPrivateProfileIntW(L"io3", L"red", 'A', filename);
    cfg->vk_green = GetPrivateProfileIntW(L"io3", L"green", 'S', filename);
    cfg->vk_blue = GetPrivateProfileIntW(L"io3", L"blue", 'D', filename);
    cfg->vk_start = GetPrivateProfileIntW(L"io3", L"start", VK_RETURN, filename);
}
