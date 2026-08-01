#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "board/config.h"
#include "board/sg-reader.h"

#include "hooklib/config.h"
#include "hooklib/dvd.h"

#include "swdchook/config.h"
#include "swdchook/swdc-dll.h"

#include "platform/config.h"
#include "platform/platform.h"
#include "util/io-path.h"


void led15070_config_load(struct led15070_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    wchar_t tmpstr[16];

    cfg->enable = GetPrivateProfileIntW(L"led15070", L"enable", 1, filename);
    cfg->port_no[0] = GetPrivateProfileIntW(L"led15070", L"portNo1", 0, filename);
    cfg->port_no[1] = GetPrivateProfileIntW(L"led15070", L"portNo2", 0, filename);
    cfg->fw_ver = GetPrivateProfileIntW(L"led15070", L"fwVer", 0x90, filename);
    /* TODO: Unknown, no firmware file available */
    cfg->fw_sum = GetPrivateProfileIntW(L"led15070", L"fwSum", 0xdead, filename);

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

void swdc_dll_config_load(
        struct swdc_dll_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    io_path_config_load(
            cfg->path,
            _countof(cfg->path),
            L"swdcio",
            L"path",
            L"SEGATOOLS_SWDCIO_PATH",
            L"swdcio.dll",
            filename);
}

void zinput_config_load(struct zinput_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"zinput", L"enable", 1, filename);
}

void swdc_hook_config_load(
        struct swdc_hook_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    platform_config_load(&cfg->platform, filename);
    aime_config_load(&cfg->aime, filename);
    swdc_dll_config_load(&cfg->dll, filename);
    zinput_config_load(&cfg->zinput, filename);
    dvd_config_load(&cfg->dvd, filename);
    io4_config_load(&cfg->io4, filename);
    ffb_config_load(&cfg->ffb, filename);
    led15070_config_load(&cfg->led15070, filename);
    vfd_config_load(&cfg->vfd, filename);
}
