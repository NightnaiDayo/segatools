#pragma once

#include <stddef.h>

#include "amex/amex.h"

#include "board/sg-reader.h"

#include "hooklib/dvd.h"
#include "hooklib/touch.h"

#include "gfxhook/config.h"

#include "platform/config.h"

#include "divahook/diva-dll.h"
#include "divahook/slider.h"

struct diva_hook_config {
    struct platform_config platform;
    struct amex_config amex;
    struct aime_config aime;
    struct dvd_config dvd;
    struct gfx_config gfx;
    struct touch_screen_config touch;
    struct diva_dll_config dll;
    struct slider_config slider;
};

void diva_dll_config_load(
        struct diva_dll_config *cfg,
        const wchar_t *filename);
void slider_config_load(struct slider_config *cfg, const wchar_t *filename);
void diva_hook_config_load(
        struct diva_hook_config *cfg,
        const wchar_t *filename);
