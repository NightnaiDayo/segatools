#pragma once

#include <stddef.h>

#include "board/config.h"
#include "board/led15093.h"

#include "hooklib/dvd.h"
#include "hooklib/touch.h"
#include "hooklib/printer_chc.h"

#include "gfxhook/config.h"

#include "apm3-dll.h"
#include "mount.h"
#include "video.h"

#include "platform/config.h"
#include "unityhook/config.h"

struct apm3_hook_config {
    struct platform_config platform;
    struct aime_config aime;
    struct dvd_config dvd;
    struct io4_config io4;
    struct vfd_config vfd;
    struct touch_screen_config touch;
    struct led15093_config led15093;
    struct apm3_dll_config dll;
    struct unity_config unity;
    struct mount_config mount;
    struct video_config video;
};

void apm3_dll_config_load(
        struct apm3_dll_config *cfg,
        const wchar_t *filename);

void apm3_hook_config_load(
        struct apm3_hook_config *cfg,
        const wchar_t *filename);
