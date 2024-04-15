#pragma once

#include <stddef.h>

#include "board/config.h"

#include "hooklib/dvd.h"
#include "hooklib/touch.h"

#include "cmhook/cm-dll.h"

#include "platform/config.h"

#include "unityhook/config.h"

struct cm_hook_config {
    struct platform_config platform;
    struct aime_config aime;
    struct dvd_config dvd;
    struct io4_config io4;
    struct vfd_config vfd;
    struct cm_dll_config dll;
    struct touch_screen_config touch;
    struct unity_config unity;
};

void cm_dll_config_load(
        struct cm_dll_config *cfg,
        const wchar_t *filename);

void cm_hook_config_load(
        struct cm_hook_config *cfg,
        const wchar_t *filename);
