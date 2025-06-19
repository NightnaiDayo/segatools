#include <windows.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "carolio/config.h"

void carol_io_config_load(
        struct carol_io_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->vk_test = GetPrivateProfileIntW(L"io3", L"test", VK_F1, filename);
    cfg->vk_service = GetPrivateProfileIntW(L"io3", L"service", VK_F2, filename);
    cfg->vk_coin = GetPrivateProfileIntW(L"io3", L"coin", VK_F3, filename);
}
