#include <assert.h>
#include <stdlib.h>

#include "board/config.h"

#include "hooklib/config.h"

#include "platform/config.h"

#include "ekthook/config.h"
#include "ekthook/ekt-dll.h"

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

void ekt_dll_config_load(
        struct ekt_dll_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    GetPrivateProfileStringW(
            L"ektio",
            L"path",
            L"",
            cfg->path,
            _countof(cfg->path),
            filename);
}

void y3_config_load(
        struct y3_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    wchar_t tmpstr[5];

    memset(cfg->firm_name_field, ' ', sizeof(cfg->firm_name_field) - 1);
    cfg->firm_name_field[sizeof(cfg->firm_name_field) - 1] = '\0';

    memset(cfg->firm_name_printer, ' ', sizeof(cfg->firm_name_printer) - 1);
    cfg->firm_name_printer[sizeof(cfg->firm_name_printer) - 1] = '\0';

    memset(cfg->target_code_field, ' ', sizeof(cfg->target_code_field) - 1);
    cfg->target_code_field[sizeof(cfg->target_code_field) - 1] = '\0';

    memset(cfg->target_code_printer, ' ', sizeof(cfg->target_code_printer) - 1);
    cfg->target_code_printer[sizeof(cfg->target_code_printer) - 1] = '\0';

    cfg->enable = GetPrivateProfileIntW(L"flatPanelReader", L"enable", 1, filename);
    cfg->port_field = GetPrivateProfileIntW(L"flatPanelReader", L"port_field", 10, filename);
    cfg->port_printer = GetPrivateProfileIntW(L"flatPanelReader", L"port_printer", 11, filename);

    cfg->dll_version = (float)GetPrivateProfileIntW(
            L"flatPanelReader",
            L"dllVersion",
            1,
            filename);

    cfg->firm_version = (float)GetPrivateProfileIntW(
            L"flatPanelReader",
            L"firmVersion",
            1,
            filename);

    GetPrivateProfileStringW(
            L"flatPanelReader",
            L"firmNameField",
            L"SFPR",
            tmpstr,
            _countof(tmpstr),
            filename);

    wcstombs(cfg->firm_name_field, tmpstr, sizeof(cfg->firm_name_field) - 1);

    GetPrivateProfileStringW(
            L"flatPanelReader",
            L"firmNamePrinter",
            L"SPRT",
            tmpstr,
            _countof(tmpstr),
            filename);

    wcstombs(cfg->firm_name_printer, tmpstr, sizeof(cfg->firm_name_printer) - 1);

    GetPrivateProfileStringW(
            L"flatPanelReader",
            L"targetCodeField",
            L"SFR0",
            tmpstr,
            _countof(tmpstr),
            filename);

    wcstombs(cfg->target_code_field, tmpstr, sizeof(cfg->target_code_field) - 1);

    GetPrivateProfileStringW(
            L"flatPanelReader",
            L"targetCodePrinter",
            L"SPT0",
            tmpstr,
            _countof(tmpstr),
            filename);

    wcstombs(cfg->target_code_printer, tmpstr, sizeof(cfg->target_code_printer) - 1);
}

void ekt_hook_config_load(
        struct ekt_hook_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    platform_config_load(&cfg->platform, filename);
    aime_config_load(&cfg->aime, filename);
    io4_config_load(&cfg->io4, filename);
    dvd_config_load(&cfg->dvd, filename);
    led15093_config_load(&cfg->led15093, filename);
    y3_config_load(&cfg->y3, filename);
    unity_config_load(&cfg->unity, filename);
    ekt_dll_config_load(&cfg->dll, filename);
}
