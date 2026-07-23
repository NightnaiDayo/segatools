#include <windows.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "divaio/config.h"

static const int diva_io_default_slider[] = {
    'Q', 'W', 'E', 'R', 'U', 'I', 'O', 'P'
};

void diva_kb_config_load(
    struct diva_kb_config* cfg,
    const wchar_t* filename) {
    wchar_t cell[32];

    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->vk_start = GetPrivateProfileIntW(L"keyboard", L"start", VK_SPACE, filename);
    cfg->vk_cross = GetPrivateProfileIntW(L"keyboard", L"cross",  VK_DOWN, filename);
    cfg->vk_circle = GetPrivateProfileIntW(L"keyboard", L"circle", VK_RIGHT, filename);
    cfg->vk_triangle = GetPrivateProfileIntW(L"keyboard", L"triangle", VK_UP, filename);
    cfg->vk_square = GetPrivateProfileIntW(L"keyboard", L"square", VK_LEFT, filename);

    cfg->vk_auto_left = GetPrivateProfileIntW(L"slider", L"autoSlideLeft", VK_LSHIFT, filename);
    cfg->vk_auto_right = GetPrivateProfileIntW(L"slider", L"autoSlideRight", VK_RSHIFT, filename);

    for (int c = 0; c < _countof(cfg->vk_slider); c++) {
        swprintf_s(cell, _countof(cell), L"cell%i", c + 1);
        cfg->vk_slider[c] = GetPrivateProfileIntW(
            L"slider",
            cell,
            diva_io_default_slider[c / 4],
            filename);
    }
}

void diva_di_config_load(struct diva_di_config* cfg, const wchar_t* filename) {
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

    cfg->test = GetPrivateProfileIntW(L"dinput", L"test", 0, filename);
    cfg->service = GetPrivateProfileIntW(L"dinput", L"service", 0, filename);

    cfg->square[0] = GetPrivateProfileIntW(L"dinput", L"square", 0, filename);
    cfg->triangle[0] = GetPrivateProfileIntW(L"dinput", L"triangle", 0, filename);
    cfg->cross[0] = GetPrivateProfileIntW(L"dinput", L"cross", 0, filename);
    cfg->circle[0] = GetPrivateProfileIntW(L"dinput", L"circle", 0, filename);
    cfg->start = GetPrivateProfileIntW(L"dinput", L"start", 0, filename);

    cfg->square[1] = GetPrivateProfileIntW(L"dinput", L"extraSquare", 0, filename);
    cfg->triangle[1] = GetPrivateProfileIntW(L"dinput", L"extraTriangle", 0, filename);
    cfg->cross[1] = GetPrivateProfileIntW(L"dinput", L"extraCross", 0, filename);
    cfg->circle[1] = GetPrivateProfileIntW(L"dinput", L"extraCircle", 0, filename);

    cfg->auto_left_button = GetPrivateProfileIntW(L"dinput", L"autoSlideLeftButton", 0, filename);
    cfg->auto_right_button = GetPrivateProfileIntW(L"dinput", L"autoSlideRightButton", 0, filename);

    GetPrivateProfileStringW(
            L"dinput",
            L"autoSlideAxis1",
            L"",
            cfg->auto_left_axis,
            _countof(cfg->auto_left_axis),
            filename);
    GetPrivateProfileStringW(
            L"dinput",
            L"autoSlideAxis2",
            L"",
            cfg->auto_right_axis,
            _countof(cfg->auto_right_axis),
            filename);

    cfg->auto_left_invert = GetPrivateProfileIntW(L"dinput", L"autoSlideAxis1Invert", 0, filename);
    cfg->auto_right_invert = GetPrivateProfileIntW(L"dinput", L"autoSlideAxis2Invert", 0, filename);

}

void diva_xi_config_load(struct diva_xi_config* cfg, const wchar_t* filename) {
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->use_select_as_test = GetPrivateProfileIntW(
        L"xinput",
        L"useSelect",
        1,
        filename);
}

void diva_io_config_load(struct diva_io_config* cfg, const wchar_t* filename) {
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->vk_test = GetPrivateProfileIntW(L"io3", L"test", VK_F1, filename);
    cfg->vk_service = GetPrivateProfileIntW(L"io3", L"service", VK_F2, filename);
    cfg->vk_coin = GetPrivateProfileIntW(L"io3", L"coin", VK_F3, filename);

    cfg->hold_transfer_time_min = GetPrivateProfileIntW(L"io3", L"holdTransferTimeMin", 500, filename);
    cfg->hold_transfer_time_max = GetPrivateProfileIntW(L"io3", L"holdTransferTimeMax", 2000, filename);
    cfg->dropped_input_frames = GetPrivateProfileIntW(L"io3", L"doubleTapReleaseDelayFrames", 1, filename);

    GetPrivateProfileStringW(
        L"io3",
        L"mode",
        L"keyboard",
        cfg->input_mode,
        _countof(cfg->input_mode),
        filename);

    /* Load touch input type. Mouse input by default. */
    GetPrivateProfileStringW(
        L"touch",
        L"mode",
        L"mouse",
        cfg->touch_mode,
        _countof(cfg->touch_mode),
        filename);

    diva_kb_config_load(&cfg->kb, filename);
    diva_di_config_load(&cfg->di, filename);
    diva_xi_config_load(&cfg->xi, filename);
}
