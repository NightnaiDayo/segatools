#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "amex/amex.h"
#include "amex/config.h"
#include "gfxhook/config.h"

#include "board/config.h"
#include "board/sg-reader.h"

#include "carolhook/config.h"

#include "platform/config.h"
#include "platform/platform.h"
#include "util/io-path.h"

void carol_dll_config_load(
        struct carol_dll_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    io_path_config_load(
            cfg->path,
            _countof(cfg->path),
            L"carolio",
            L"path",
            L"SEGATOOLS_CAROLIO_PATH",
            L"carolio.dll",
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

void controlbd_config_load(
        struct controlbd_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(
            L"controlbd",
            L"enable",
            1,
            filename);
}

void ledbd_config_load(
        struct ledbd_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(
            L"ledbd",
            L"enable",
            1,
            filename);
}


void carol_hook_config_load(
        struct carol_hook_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    platform_config_load(&cfg->platform, filename);
    amex_config_load(&cfg->amex, filename);
    aime_config_load(&cfg->aime, filename);
    carol_dll_config_load(&cfg->dll, filename);
    gfx_config_load(&cfg->gfx, filename);
    touch_config_load(&cfg->touch, filename);
    controlbd_config_load(&cfg->controlbd, filename);
    ledbd_config_load(&cfg->ledbd, filename);
}
