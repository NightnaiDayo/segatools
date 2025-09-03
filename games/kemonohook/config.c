#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "amex/config.h"

#include "board/config.h"

#include "hooklib/config.h"
#include "hooklib/dvd.h"

#include "kemonohook/config.h"

#include "platform/config.h"

// Check windows
#if _WIN32 || _WIN64
    #if _WIN64
        #define ENV64BIT
    #else
        #define ENV32BIT
    #endif
#endif

// Check GCC
#if __GNUC__
    #if __x86_64__ || __ppc64__
        #define ENV64BIT
    #else
        #define ENV32BIT
    #endif
#endif

void kemono_dll_config_load(
        struct kemono_dll_config *cfg,
        const wchar_t *filename) {
    assert(cfg != NULL);
    assert(filename != NULL);

    #if defined(ENV32BIT)
        // Always empty, due to amdaemon being 64 bit in 32 bit mode
        memset(cfg->path, 0, sizeof(cfg->path));
    #elif defined(ENV64BIT)
        GetPrivateProfileStringW(
                L"kemonoio",
                L"path",
                L"",
                cfg->path,
                _countof(cfg->path),
                filename);
    #else
        #error "Unknown environment"
    #endif
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
    cfg->port_no[1] = GetPrivateProfileIntW(L"led15093", L"portNo2", 1, filename);
    cfg->high_baudrate = GetPrivateProfileIntW(L"led15093", L"highBaud", 0, filename);
    cfg->fw_ver = GetPrivateProfileIntW(L"led15093", L"fwVer", 0xA0, filename);
    cfg->fw_sum = GetPrivateProfileIntW(L"led15093", L"fwSum", 0xAA53, filename);

    GetPrivateProfileStringW(
            L"led15093",
            L"boardNumber",
            L"15093-04",
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
            L"6704 ",
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
            L"6704 ",
            tmpstr,
            _countof(tmpstr),
            filename);

    n = wcstombs(cfg->boot_chip_number, tmpstr, sizeof(cfg->boot_chip_number));
    for (int i = n; i < sizeof(cfg->boot_chip_number); i++)
    {
        cfg->boot_chip_number[i] = ' ';
    }
}

void kemono_hook_config_load(
        struct kemono_hook_config *cfg,
        const wchar_t *filename) {
    assert(cfg != NULL);
    assert(filename != NULL);

    platform_config_load(&cfg->platform, filename);
    aime_config_load(&cfg->aime, filename);
    dvd_config_load(&cfg->dvd, filename);
    vfd_config_load(&cfg->vfd, filename);
    kemono_dll_config_load(&cfg->dll, filename);
    unity_config_load(&cfg->unity, filename);
    printer_chc_config_load(&cfg->printer, filename);
    amex_config_load(&cfg->amex, filename);
    led15093_config_load(&cfg->led15093, filename);
}
