#include <windows.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "apm3io/config.h"

const int BUTTON_DEFAULTS[] = {'Q','W','E','R','A','S','D','F'};

void apm3_kb_config_load(
        struct apm3_kb_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->vk_start = GetPrivateProfileIntW(L"keyboard", L"start", 'P', filename);
    cfg->vk_home = GetPrivateProfileIntW(L"keyboard", L"home", 'O', filename);

    cfg->vk_up = GetPrivateProfileIntW(L"keyboard", L"up", VK_UP, filename);
    cfg->vk_right = GetPrivateProfileIntW(L"keyboard", L"right", VK_RIGHT, filename);
    cfg->vk_down = GetPrivateProfileIntW(L"keyboard", L"down", VK_DOWN, filename);
    cfg->vk_left = GetPrivateProfileIntW(L"keyboard", L"left", VK_LEFT, filename);

    wchar_t tmp[16];
    for (int i = 0; i < APM3_BUTTON_COUNT; i++) {
        swprintf_s(tmp, 32, L"button%d", i + 1);
        cfg->vk_buttons[i] = GetPrivateProfileIntW(L"keyboard", tmp, BUTTON_DEFAULTS[i], filename);
    }
}

void apm3_di_config_load(struct apm3_di_config *cfg, const wchar_t *filename)
{
    wchar_t key[8];
    int i;

    assert(cfg != NULL);
    assert(filename != NULL);

    GetPrivateProfileStringW(
            L"dinput",
            L"deviceName",
            L"",
            cfg->device_name,
            _countof(cfg->device_name),
            filename);

    cfg->home = GetPrivateProfileIntW(L"dinput", L"home", 0, filename);
    cfg->start = GetPrivateProfileIntW(L"dinput", L"start", 0, filename);

    for (i = 0 ; i < 8 ; i++) {
        swprintf_s(key, _countof(key), L"button%i", i + 1);
        cfg->button[i] = GetPrivateProfileIntW(L"dinput", key, i + 1, filename);
    }

}

void apm3_xi_config_load(struct apm3_xi_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->analog_stick_enabled = GetPrivateProfileIntW(
                                L"xinput",
                                L"analogStickEnabled",
                                1,
                                filename);
}

void apm3_io_config_load(
        struct apm3_io_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->vk_test = GetPrivateProfileIntW(L"io4", L"test", VK_F1, filename);
    cfg->vk_service = GetPrivateProfileIntW(L"io4", L"service", VK_F2, filename);
    cfg->vk_coin = GetPrivateProfileIntW(L"io4", L"coin", VK_F3, filename);

    GetPrivateProfileStringW(
            L"io4",
            L"mode",
            L"",
            cfg->mode,
            _countof(cfg->mode),
            filename);
    
    apm3_kb_config_load(&cfg->kb, filename);
    apm3_di_config_load(&cfg->di, filename);
    apm3_xi_config_load(&cfg->xi, filename);
}
