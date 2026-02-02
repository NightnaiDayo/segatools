#pragma once

#include <stddef.h>

#include "board/config.h"
#include "board/led15093.h"

#include "hooklib/dvd.h"
#include "hooklib/touch.h"

#include "gfxhook/config.h"

#include "soulhook/soul-dll.h"
#include "soulhook/zinput.h"

#include "platform/config.h"

struct soul_hook_config {
    struct platform_config platform;
    struct io4_config io4;
    struct aime_config aime;
    struct dvd_config dvd;
    struct vfd_config vfd;
    struct touch_screen_config touch;
    struct led15093_config led15093;
    struct gfx_config gfx;
    struct soul_dll_config dll;
    struct zinput_config zinput;
};

void soul_dll_config_load(
        struct soul_dll_config *cfg,
        const wchar_t *filename);

void soul_hook_config_load(
        struct soul_hook_config *cfg,
        const wchar_t *filename);
