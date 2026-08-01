#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "board/config.h"

#include "hooklib/config.h"
#include "hooklib/dvd.h"

#include "cmhook/config.h"

#include "platform/config.h"
#include "util/io-path.h"

void cm_dll_config_load(
        struct cm_dll_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    io_path_config_load(
            cfg->path,
            _countof(cfg->path),
            L"cmio",
            L"path",
            L"SEGATOOLS_CMIO_PATH",
            L"cmio.dll",
            filename);
}

void cm_hook_config_load(
        struct cm_hook_config *cfg,
        const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    platform_config_load(&cfg->platform, filename);
    aime_config_load(&cfg->aime, filename);
    dvd_config_load(&cfg->dvd, filename);
    io4_config_load(&cfg->io4, filename);
    vfd_config_load(&cfg->vfd, filename);
    touch_screen_config_load(&cfg->touch, filename);
    printer_chc_config_load(&cfg->printer, filename);
    cm_dll_config_load(&cfg->dll, filename);
    unity_config_load(&cfg->unity, filename);
}
