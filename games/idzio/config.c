#include <windows.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "idzio/config.h"

void idz_di_config_load(struct idz_di_config *cfg, const wchar_t *filename)
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

    GetPrivateProfileStringW(
            L"dinput",
            L"pedalsName",
            L"",
            cfg->pedals_name,
            _countof(cfg->pedals_name),
            filename);

    GetPrivateProfileStringW(
            L"dinput",
            L"shifterName",
            L"",
            cfg->shifter_name,
            _countof(cfg->shifter_name),
            filename);

    GetPrivateProfileStringW(
            L"dinput",
            L"brakeAxis",
            L"RZ",
            cfg->brake_axis,
            _countof(cfg->brake_axis),
            filename);

    GetPrivateProfileStringW(
            L"dinput",
            L"accelAxis",
            L"Y",
            cfg->accel_axis,
            _countof(cfg->accel_axis),
            filename);

    cfg->start = GetPrivateProfileIntW(L"dinput", L"start", 0, filename);
    cfg->view_chg = GetPrivateProfileIntW(L"dinput", L"viewChg", 0, filename);
    cfg->shift_dn = GetPrivateProfileIntW(L"dinput", L"shiftDn", 0, filename);
    cfg->shift_up = GetPrivateProfileIntW(L"dinput", L"shiftUp", 0, filename);

    cfg->reverse_brake_axis = GetPrivateProfileIntW(
                            L"dinput",
                            L"reverseBrakeAxis",
                            0,
                            filename);
    cfg->reverse_accel_axis = GetPrivateProfileIntW(
                            L"dinput",
                            L"reverseAccelAxis",
                            0,
                            filename);

    for (i = 0 ; i < 6 ; i++) {
        swprintf_s(key, _countof(key), L"gear%i", i + 1);
        cfg->gear[i] = GetPrivateProfileIntW(L"dinput", key, i + 1, filename);
    }
    
    // FFB configuration
    cfg->ffb_constant_force_strength = GetPrivateProfileIntW(
            L"dinput",
            L"constantForceStrength",
            100,
            filename);

    cfg->ffb_rumble_strength = GetPrivateProfileIntW(
            L"dinput",
            L"rumbleStrength",
            100,
            filename);

    cfg->ffb_damper_strength = GetPrivateProfileIntW(
            L"dinput",
            L"damperStrength",
            100,
            filename);

    cfg->ffb_rumble_duration = GetPrivateProfileIntW(
            L"dinput",
            L"rumbleDuration",
            1000,
            filename);
}

void idz_xi_config_load(struct idz_xi_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->single_stick_steering = GetPrivateProfileIntW(
                                L"xinput",
                                L"singleStickSteering",
                                1,
                                filename);

    cfg->linear_steering = GetPrivateProfileIntW(
                           L"xinput",
                           L"linearSteering",
                           0,
                           filename);
    
    cfg->left_stick_deadzone = GetPrivateProfileIntW(
                               L"xinput",
                               L"leftStickDeadzone",
                               7849,
                               filename);
    
    cfg->right_stick_deadzone = GetPrivateProfileIntW(
                                L"xinput",
                                L"rightStickDeadzone",
                                8689,
                                filename);
}

void idz_io_config_load(struct idz_io_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    /* Technically it's io4 */
    cfg->vk_test = GetPrivateProfileIntW(L"io3", L"test", VK_F1, filename);
    cfg->vk_service = GetPrivateProfileIntW(L"io3", L"service", VK_F2, filename);
    cfg->vk_coin = GetPrivateProfileIntW(L"io3", L"coin", VK_F3, filename);
    cfg->restrict_ = GetPrivateProfileIntW(L"io3", L"restrict", 97, filename);

    GetPrivateProfileStringW(
            L"io3",
            L"mode",
            L"xinput",
            cfg->mode,
            _countof(cfg->mode),
            filename);

    idz_shifter_config_load(&cfg->shifter, filename);
    idz_di_config_load(&cfg->di, filename);
    idz_xi_config_load(&cfg->xi, filename);
}

void idz_shifter_config_load(
        struct idz_shifter_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->auto_neutral = GetPrivateProfileIntW(
            L"io3",
            L"autoNeutral",
            0,
            filename);
}
