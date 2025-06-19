#include <windows.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "tokyoio/config.h"


void tokyo_kb_config_load(
        struct tokyo_kb_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    /* Load game button keyboard bindings */
    cfg->vk_push_left_b = GetPrivateProfileIntW(L"keyboard", L"leftBlue", 'A', filename);
    cfg->vk_push_center_y = GetPrivateProfileIntW(L"keyboard", L"centerYellow", 'S', filename);
    cfg->vk_push_right_r = GetPrivateProfileIntW(L"keyboard", L"rightRed", 'D', filename);

    /* Load sensor keyboard bindings */
    cfg->vk_foot_l = GetPrivateProfileIntW(L"keyboard", L"footLeft", VK_LEFT, filename);
    cfg->vk_foot_r = GetPrivateProfileIntW(L"keyboard", L"footRight", VK_RIGHT, filename);
    cfg->vk_jump_1 = GetPrivateProfileIntW(L"keyboard", L"jump1", 'Z', filename);
    cfg->vk_jump_2 = GetPrivateProfileIntW(L"keyboard", L"jump2", 'X', filename);
    cfg->vk_jump_3 = GetPrivateProfileIntW(L"keyboard", L"jump3", 'C', filename);
    cfg->vk_jump_4 = GetPrivateProfileIntW(L"keyboard", L"jump4", 'B', filename);
    cfg->vk_jump_5 = GetPrivateProfileIntW(L"keyboard", L"jump5", 'N', filename);
    cfg->vk_jump_6 = GetPrivateProfileIntW(L"keyboard", L"jump6", 'M', filename);
    cfg->vk_jump_all = GetPrivateProfileIntW(L"keyboard", L"jumpAll", VK_SPACE, filename);
}

void tokyo_io_config_load(
        struct tokyo_io_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->vk_test = GetPrivateProfileIntW(L"io4", L"test", '1', filename);
    cfg->vk_service = GetPrivateProfileIntW(L"io4", L"service", '2', filename);
    cfg->vk_coin = GetPrivateProfileIntW(L"io4", L"coin", '3', filename);

    GetPrivateProfileStringW(
            L"io4",
            L"mode",
            L"xinput",
            cfg->mode,
            _countof(cfg->mode),
            filename);

    tokyo_kb_config_load(&cfg->kb, filename);
}
