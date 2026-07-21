#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "hkbio/config.h"

#include <xinput.h>


void hkb_kb_config_load(struct hkb_kb_config* cfg, const wchar_t* filename) {
    cfg->vk_terminal_decide = GetPrivateProfileIntW(L"keyboard", L"decide", 'A', filename);
    cfg->vk_terminal_cancel = GetPrivateProfileIntW(L"keyboard", L"cancel", 'S', filename);
    cfg->vk_terminal_up = GetPrivateProfileIntW(L"keyboard", L"up", VK_UP, filename);
    cfg->vk_terminal_right = GetPrivateProfileIntW(L"keyboard", L"right", VK_RIGHT, filename);
    cfg->vk_terminal_down = GetPrivateProfileIntW(L"keyboard", L"down", VK_DOWN, filename);
    cfg->vk_terminal_left = GetPrivateProfileIntW(L"keyboard", L"left", VK_LEFT, filename);
    cfg->vk_terminal_left_2 = GetPrivateProfileIntW(L"keyboard", L"left2", 'Q', filename);
    cfg->vk_terminal_right_2 = GetPrivateProfileIntW(L"keyboard", L"right2", 'W', filename);
}

void hkb_di_config_load(struct hkb_di_config* cfg, const wchar_t* filename) {
    assert(cfg != NULL);
    assert(filename != NULL);

    GetPrivateProfileStringW(L"dinput", L"deviceName", L"", cfg->device_name, _countof(cfg->device_name), filename);

    cfg->test = GetPrivateProfileIntW(L"dinput", L"test", 0, filename);
    cfg->service = GetPrivateProfileIntW(L"dinput", L"service", 0, filename);

    cfg->up = GetPrivateProfileIntW(L"dinput", L"up", 0, filename);
    cfg->right = GetPrivateProfileIntW(L"dinput", L"right", 0, filename);
    cfg->down = GetPrivateProfileIntW(L"dinput", L"down", 0, filename);
    cfg->left = GetPrivateProfileIntW(L"dinput", L"left", 0, filename);

    cfg->decide = GetPrivateProfileIntW(L"dinput", L"decide", 0, filename);
    cfg->cancel = GetPrivateProfileIntW(L"dinput", L"cancel", 0, filename);
    cfg->left_2 = GetPrivateProfileIntW(L"dinput", L"left2", 0, filename);
    cfg->right_2 = GetPrivateProfileIntW(L"dinput", L"right2", 0, filename);
}

void hkb_io_config_load(struct hkb_io_config* cfg, const wchar_t* filename) {
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->vk_test = GetPrivateProfileIntW(L"io4", L"test", VK_F1, filename);
    cfg->vk_service = GetPrivateProfileIntW(L"io4", L"service", VK_F2, filename);
    cfg->vk_coin = GetPrivateProfileIntW(L"io4", L"coin", VK_F3, filename);

    GetPrivateProfileStringW(L"io4", L"mode", L"keyboard", cfg->mode, _countof(cfg->mode), filename);

    hkb_kb_config_load(&cfg->kb, filename);
    hkb_di_config_load(&cfg->di, filename);
}
