#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "board/config.h"

#include "hooklib/config.h"
#include "hooklib/dvd.h"

#include "mai2hook/config.h"

#include "platform/config.h"
#include "util/io-path.h"

void mai2_dll_config_load(
    struct mai2_dll_config *cfg,
    const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    io_path_config_load(
        cfg->path,
        _countof(cfg->path),
        L"mai2io",
        L"path",
        L"SEGATOOLS_MAI2IO_PATH",
        L"mai2io.dll",
        filename);
}

void touch_config_load(
    struct touch_config *cfg,
    const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable_1p = GetPrivateProfileIntW(L"touch", L"p1Enable", 1, filename);
    cfg->enable_2p = GetPrivateProfileIntW(L"touch", L"p2Enable", 1, filename);
}

void led15070_config_load(struct led15070_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    wchar_t tmpstr[16];

    cfg->enable = GetPrivateProfileIntW(L"led15070", L"enable", 1, filename);
    cfg->port_no[0] = GetPrivateProfileIntW(L"led15070", L"portNo1", 0, filename);
    cfg->port_no[1] = GetPrivateProfileIntW(L"led15070", L"portNo2", 0, filename);
    cfg->fw_ver = GetPrivateProfileIntW(L"led15070", L"fwVer", 0x90, filename);
    cfg->fw_sum = GetPrivateProfileIntW(L"led15070", L"fwSum", 0x00, filename);

    GetPrivateProfileStringW(
        L"led15070",
        L"boardNumber",
        L"15070-04",
        tmpstr,
        _countof(tmpstr),
        filename);

    size_t n = wcstombs(cfg->board_number, tmpstr, sizeof(cfg->board_number));
    for (int i = n; i < sizeof(cfg->board_number); i++)
    {
        cfg->board_number[i] = ' ';
    }

    GetPrivateProfileStringW(
        L"led15070",
        L"eepromPath",
        L"DEVICE",
        cfg->eeprom_path,
        _countof(cfg->eeprom_path),
        filename);
}

void mai2_hook_config_load(
    struct mai2_hook_config *cfg,
    const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    platform_config_load(&cfg->platform, filename);
    aime_config_load(&cfg->aime, filename);
    dvd_config_load(&cfg->dvd, filename);
    io4_config_load(&cfg->io4, filename);
    vfd_config_load(&cfg->vfd, filename);
    mai2_dll_config_load(&cfg->dll, filename);
    touch_config_load(&cfg->touch, filename);
    led15070_config_load(&cfg->led15070, filename);
    unity_config_load(&cfg->unity, filename);
}
