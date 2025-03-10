#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "fgoio/config.h"

#include <xinput.h>


void fgo_kb_config_load(
        struct fgo_kb_config *cfg,
        const wchar_t *filename) {

    cfg->vk_attack = GetPrivateProfileIntW(L"keyboard", L"attack", ' ', filename);
    cfg->vk_dash = GetPrivateProfileIntW(L"keyboard", L"dash", VK_LSHIFT, filename);
    cfg->vk_target = GetPrivateProfileIntW(L"keyboard", L"target", 'J', filename);
    cfg->vk_camera = GetPrivateProfileIntW(L"keyboard", L"camera", 'K', filename);
    cfg->vk_np = GetPrivateProfileIntW(L"keyboard", L"np", 'L', filename);

    // Standard WASD
    cfg->vk_right = GetPrivateProfileIntW(L"keyboard", L"right", 'D', filename);
    cfg->vk_left = GetPrivateProfileIntW(L"keyboard", L"left", 'A', filename);
    cfg->vk_down = GetPrivateProfileIntW(L"keyboard", L"down", 'S', filename);
    cfg->vk_up = GetPrivateProfileIntW(L"keyboard", L"up", 'W', filename);
}

void fgo_xi_config_load(
        struct fgo_xi_config *cfg,
        const wchar_t *filename) {

    cfg->stick_deadzone = GetPrivateProfileIntW(L"xinput", L"stickDeadzone", XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, filename);

}

void fgo_io_config_load(
        struct fgo_io_config *cfg,
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
            L"xinput",
            cfg->mode,
            _countof(cfg->mode),
            filename);

    fgo_xi_config_load(&cfg->xi, filename);
    fgo_kb_config_load(&cfg->kb, filename);
}
