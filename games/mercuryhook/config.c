#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "board/config.h"

#include "hooklib/config.h"
#include "hooklib/dvd.h"
#include "gfxhook/config.h"

#include "mercuryhook/config.h"

#include "platform/config.h"
#include "util/io-path.h"

void mercury_dll_config_load(
        struct mercury_dll_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    io_path_config_load(
            cfg->path,
            _countof(cfg->path),
            L"mercuryio",
            L"path",
            L"SEGATOOLS_MERCURYIO_PATH",
            L"mercuryio.dll",
            filename);
}

void touch_config_load(
        struct touch_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(
            L"touch",
            L"enable",
            1,
            filename);
}

void elizabeth_config_load(
        struct elizabeth_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(
            L"elizabeth",
            L"enable",
            1,
            filename);
}


void mercury_hook_config_load(
        struct mercury_hook_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    platform_config_load(&cfg->platform, filename);
    aime_config_load(&cfg->aime, filename);
    dvd_config_load(&cfg->dvd, filename);
    io4_config_load(&cfg->io4, filename);
    gfx_config_load(&cfg->gfx, filename);
    vfd_config_load(&cfg->vfd, filename);
    mercury_dll_config_load(&cfg->dll, filename);
    touch_config_load(&cfg->touch, filename);
    elizabeth_config_load(&cfg->elizabeth, filename);
}
