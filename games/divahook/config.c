#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "amex/amex.h"
#include "amex/config.h"

#include "board/config.h"
#include "board/sg-reader.h"

#include "hooklib/config.h"
#include "hooklib/dvd.h"

#include "divahook/config.h"

#include "platform/config.h"
#include "platform/platform.h"
#include "util/io-path.h"

void diva_dll_config_load(
        struct diva_dll_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    io_path_config_load(
            cfg->path,
            _countof(cfg->path),
            L"divaio",
            L"path",
            L"SEGATOOLS_DIVAIO_PATH",
            L"divaio.dll",
            filename);
}

void slider_config_load(struct slider_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"slider", L"enable", 1, filename);
}

void elo_config_load(struct elo_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"touch", L"enable", 1, filename);
    cfg->port_no = GetPrivateProfileIntW(L"touch", L"port_no", 0, filename);
}

void diva_hook_config_load(
        struct diva_hook_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    platform_config_load(&cfg->platform, filename);
    amex_config_load(&cfg->amex, filename);
    aime_config_load(&cfg->aime, filename);
    dvd_config_load(&cfg->dvd, filename);
    gfx_config_load(&cfg->gfx, filename);
    diva_dll_config_load(&cfg->dll, filename);
    slider_config_load(&cfg->slider, filename);
    elo_config_load(&cfg->touch, filename);
}
