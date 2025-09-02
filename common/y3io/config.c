#include "y3io/config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

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

void y3_dll_config_load(
        struct y3_dll_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    GetPrivateProfileStringW(
            L"y3io",
            L"path",
            L"",
            cfg->path,
            _countof(cfg->path),
            filename);
}