#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>

#include "hooklib/config.h"
#include "hooklib/dvd.h"
#include "hooklib/y3.h"
#include "hooklib/y3-dll.h"

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

void printer_chc_config_load(struct printer_chc_config *cfg, const wchar_t *filename)
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

void printer_cx_config_load(struct printer_cx_config *cfg, const wchar_t *filename){
    assert(cfg != NULL);
    assert(filename != NULL);

    char filenameA[MAX_PATH];

    size_t n = wcstombs(filenameA, filename, MAX_PATH);
    for (int i = n; i < MAX_PATH; i++)
    {
        filenameA[i] = '\0';
    }

    cfg->enable = GetPrivateProfileIntW(L"printer", L"enable", 1, filename);

    GetPrivateProfileStringA(
            "printer",
            "firmwareVersion",
            "V04-03B",
            cfg->printer_firm_version,
            _countof(cfg->printer_firm_version),
            filenameA);

    GetPrivateProfileStringA(
            "printer",
            "configVersion",
            "V01-75",
            cfg->printer_config_version,
            _countof(cfg->printer_config_version),
            filenameA);

    GetPrivateProfileStringA(
            "printer",
            "tableVersion",
            "V01-E0",
            cfg->printer_table_version,
            _countof(cfg->printer_table_version),
            filenameA);

    GetPrivateProfileStringA(
            "printer",
            "cameraVersion",
            "00.19",
            cfg->printer_camera_version,
            _countof(cfg->printer_camera_version),
            filenameA);

    GetPrivateProfileStringW(
            L"printer",
            L"printerOutPath",
            L"DEVICE\\print",
            cfg->printer_out_path,
            _countof(cfg->printer_out_path),
            filename);

    GetPrivateProfileStringW(
            L"printer",
            L"printerDataPath",
            L"DEVICE\\cx7000_data.bin",
            cfg->printer_data_path,
            _countof(cfg->printer_data_path),
            filename);

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