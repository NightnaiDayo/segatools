#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>

#include "hooklib/config.h"
#include "hooklib/dvd.h"

void dvd_config_load(struct dvd_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"dvd", L"enable", 1, filename);
}

void touch_screen_config_load(struct touch_screen_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"touch", L"enable", 1, filename);
    cfg->remap = GetPrivateProfileIntW(L"touch", L"remap", 1, filename);
    cfg->cursor = GetPrivateProfileIntW(L"touch", L"cursor", 1, filename);
}

void printer_config_load(struct printer_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    wchar_t tmpstr[16];

    cfg->enable = GetPrivateProfileIntW(L"printer", L"enable", 0, filename);
    cfg->rotate_180 = GetPrivateProfileIntW(L"printer", L"rotate180", 0, filename);

    GetPrivateProfileStringW(
        L"printer",
        L"serial_no",
        L"5A-A123",
        tmpstr,
        _countof(tmpstr),
        filename);

    size_t n = wcstombs(cfg->serial_no, tmpstr, sizeof(cfg->serial_no));
    for (int i = n; i < sizeof(cfg->serial_no); i++)
    {
        cfg->serial_no[i] = '\0';
    }

    GetPrivateProfileStringW(
            L"printer",
            L"mainFwPath",
            L"DEVICE\\printer_main_fw.bin",
            cfg->main_fw_path,
            _countof(cfg->main_fw_path),
            filename);

    GetPrivateProfileStringW(
            L"printer",
            L"dspFwPath",
            L"DEVICE\\printer_dsp_fw.bin",
            cfg->dsp_fw_path,
            _countof(cfg->dsp_fw_path),
            filename);

    GetPrivateProfileStringW(
            L"printer",
            L"paramFwPath",
            L"DEVICE\\printer_param_fw.bin",
            cfg->param_fw_path,
            _countof(cfg->param_fw_path),
            filename);

    GetPrivateProfileStringW(
            L"printer",
            L"printerOutPath",
            L"DEVICE\\print",
            cfg->printer_out_path,
            _countof(cfg->printer_out_path),
            filename);

    cfg->wait_time = GetPrivateProfileIntW(L"printer", L"waitTime", 0, filename);
}
