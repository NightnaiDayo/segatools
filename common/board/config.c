#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "board/aime-dll.h"
#include "board/config.h"
#include "board/sg-reader.h"
#include "board/vfd.h"

#include "util/dprintf.h"

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

static void aime_dll_config_load(struct aime_dll_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    // Workaround for x64/x86 external IO dlls
    // path32 for 32bit, path64 for 64bit
    // for else.. is that possible? idk

    if (cfg->path64) {
        #if defined(ENV32BIT)
            // Always empty, due to amdaemon being 64 bit in 32 bit mode
            memset(cfg->path, 0, sizeof(cfg->path));
        #elif defined(ENV64BIT)
            GetPrivateProfileStringW(
                    L"aimeio",
                    L"path",
                    L"",
                    cfg->path,
                    _countof(cfg->path),
                    filename);
        #else
            #error "Unknown environment"
        #endif
    } else {
        GetPrivateProfileStringW(
            L"aimeio",
            L"path",
            L"",
            cfg->path,
            _countof(cfg->path),
            filename);
    }
}

void aime_config_load(struct aime_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    aime_dll_config_load(&cfg->dll, filename);
    cfg->enable = GetPrivateProfileIntW(L"aime", L"enable", 1, filename);
    cfg->port_no = GetPrivateProfileIntW(L"aime", L"portNo", 0, filename);
    cfg->high_baudrate = GetPrivateProfileIntW(L"aime", L"highBaud", 1, filename);
    cfg->gen = GetPrivateProfileIntW(L"aime", L"gen", 0, filename);
    cfg->proxy_flag = GetPrivateProfileIntW(L"aime", L"proxyFlag", 2, filename);

    GetPrivateProfileStringW(
            L"aime",
            L"authdataPath",
            L"DEVICE\\authdata.bin",
            cfg->authdata_path,
            _countof(cfg->authdata_path),
            filename);
}

void io4_config_load(struct io4_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"io4", L"enable", 1, filename);
    cfg->foreground_only = GetPrivateProfileIntW(L"io4", L"foreground", 0, filename);
}

void vfd_config_load(struct vfd_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"vfd", L"enable", 1, filename);
    cfg->port_no = GetPrivateProfileIntW(L"vfd", L"portNo", 0, filename);
    cfg->utf_conversion = GetPrivateProfileIntW(L"vfd", L"utfConversion", 0, filename);
}

void ffb_config_load(struct ffb_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"ffb", L"enable", 1, filename);
}
