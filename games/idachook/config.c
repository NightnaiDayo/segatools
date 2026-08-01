#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "board/config.h"
#include "board/sg-reader.h"

#include "hooklib/config.h"
#include "hooklib/dvd.h"

#include "idachook/config.h"
#include "idachook/idac-dll.h"

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
    cfg->fw_sum = GetPrivateProfileIntW(L"led15070", L"fwSum", 0x0000, filename);

    GetPrivateProfileStringW(
            L"led15070",
            L"boardNumber",
            L"15070-02",
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

void idac_dll_config_load(
        struct idac_dll_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    io_path_config_load(
            cfg->path,
            _countof(cfg->path),
            L"idacio",
            L"path",
            L"SEGATOOLS_IDACIO_PATH",
            L"idacio.dll",
            filename);
}

void indrun_config_load(
        struct indrun_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"indrun", L"enable", 1, filename);
    GetPrivateProfileStringW(
        L"indrun",
        L"spike",
        L"",
        cfg->patch_file,
        _countof(cfg->patch_file),
        filename);
}

void idac_hook_config_load(
        struct idac_hook_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    platform_config_load(&cfg->platform, filename);
    aime_config_load(&cfg->aime, filename);
    idac_dll_config_load(&cfg->dll, filename);
    dvd_config_load(&cfg->dvd, filename);
    io4_config_load(&cfg->io4, filename);
    ffb_config_load(&cfg->ffb, filename);
    led15070_config_load(&cfg->led15070, filename);
    indrun_config_load(&cfg->indrun, filename);
}
