#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "amex/amex.h"
#include "amex/config.h"

#include "board/config.h"
#include "board/sg-reader.h"

#include "chunihook/config.h"

#include "gfxhook/config.h"

#include "hooklib/config.h"

#include "platform/config.h"
#include "platform/platform.h"

void chuni_dll_config_load(
        struct chuni_dll_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    GetPrivateProfileStringW(
            L"chuniio",
            L"path",
            L"",
            cfg->path,
            _countof(cfg->path),
            filename);
}

void slider_config_load(struct slider_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"slider", L"enable", 1, filename);
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
    cfg->fw_ver = GetPrivateProfileIntW(L"led15093", L"fwVer", 0x90, filename);
    cfg->fw_sum = GetPrivateProfileIntW(L"led15093", L"fwSum", 0xadf7, filename);

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
            L"6710 ",
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

void chuni_hook_config_load(
        struct chuni_hook_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    memset(cfg, 0, sizeof(*cfg));

    platform_config_load(&cfg->platform, filename);
    amex_config_load(&cfg->amex, filename);
    aime_config_load(&cfg->aime, filename);
    gfx_config_load(&cfg->gfx, filename);
    chuni_dll_config_load(&cfg->dll, filename);
    slider_config_load(&cfg->slider, filename);
    led15093_config_load(&cfg->led15093, filename);
}
