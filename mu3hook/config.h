#pragma once

#include <stddef.h>

#include "board/config.h"
// #include "board/led15093.h"

#include "gfxhook/gfx.h"

#include "hooklib/dvd.h"

#include "mu3hook/mu3-dll.h"

#include "platform/config.h"

#include "unityhook/config.h"

struct mu3_hook_config {
    struct platform_config platform;
    struct aime_config aime;
    struct dvd_config dvd;
    struct io4_config io4;
    struct gfx_config gfx;
    // struct led15093_config led15093;
    struct vfd_config vfd;
    struct mu3_dll_config dll;
    struct unity_config unity;
};

void mu3_dll_config_load(
        struct mu3_dll_config *cfg,
        const wchar_t *filename);

void mu3_hook_config_load(
        struct mu3_hook_config *cfg,
        const wchar_t *filename);
