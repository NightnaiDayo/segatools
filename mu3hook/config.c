#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "board/config.h"

#include "gfxhook/config.h"

#include "hooklib/config.h"
#include "hooklib/dvd.h"

#include "mu3hook/config.h"

#include "platform/config.h"

void mu3_dll_config_load(
        struct mu3_dll_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    GetPrivateProfileStringW(
            L"mu3io",
            L"path",
            L"",
            cfg->path,
            _countof(cfg->path),
            filename);
}

void led15093_config_load(struct led15093_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    wchar_t tmpstr[16];

    memset(cfg->board_number, ' ', sizeof(cfg->board_number));
    memset(cfg->chip_number, ' ', sizeof(cfg->chip_number));
    memset(cfg->boot_chip_number, ' ', sizeof(cfg->boot_chip_number));

    cfg->enable = GetPrivateProfileIntW(L"led15093", L"enable", 1, filename);
    cfg->port_no[0] = GetPrivateProfileIntW(L"led15093", L"portNo1", 0, filename);
    cfg->port_no[1] = GetPrivateProfileIntW(L"led15093", L"portNo2", 0, filename);
    cfg->high_baudrate = GetPrivateProfileIntW(L"led15093", L"highBaud", 0, filename);
    cfg->fw_ver = GetPrivateProfileIntW(L"led15093", L"fwVer", 0xA0, filename);
    cfg->fw_sum = GetPrivateProfileIntW(L"led15093", L"fwSum", 0xAA53, filename);

    GetPrivateProfileStringW(
            L"led15093",
            L"boardNumber",
            L"15093-06",
            tmpstr,
            _countof(tmpstr),
            filename);

    size_t n = wcstombs(cfg->board_number, tmpstr, sizeof(cfg->board_number));
    for (int i = n; i < sizeof(cfg->board_number); i++)
    {
        cfg->board_number[i] = ' ';
    }

    GetPrivateProfileStringW(
            L"led15093",
            L"chipNumber",
            L"6710A",
            tmpstr,
            _countof(tmpstr),
            filename);

    n = wcstombs(cfg->chip_number, tmpstr, sizeof(cfg->chip_number));
    for (int i = n; i < sizeof(cfg->chip_number); i++)
    {
        cfg->chip_number[i] = ' ';
    }

    GetPrivateProfileStringW(
            L"led15093",
            L"bootChipNumber",
            L"6709 ",
            tmpstr,
            _countof(tmpstr),
            filename);

    n = wcstombs(cfg->boot_chip_number, tmpstr, sizeof(cfg->boot_chip_number));
    for (int i = n; i < sizeof(cfg->boot_chip_number); i++)
    {
        cfg->boot_chip_number[i] = ' ';
    }
}

void mu3_hook_config_load(
        struct mu3_hook_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    platform_config_load(&cfg->platform, filename);
    aime_config_load(&cfg->aime, filename);
    dvd_config_load(&cfg->dvd, filename);
    io4_config_load(&cfg->io4, filename);
    gfx_config_load(&cfg->gfx, filename);
    led15093_config_load(&cfg->led15093, filename);
    vfd_config_load(&cfg->vfd, filename);
    mu3_dll_config_load(&cfg->dll, filename);
    unity_config_load(&cfg->unity, filename);
}
