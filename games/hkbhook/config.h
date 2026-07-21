#pragma once

#include <stddef.h>

#include "board/config.h"
#include "board/led15093.h"

#include "hooklib/dvd.h"
#include "hooklib/touch.h"

#include "gfxhook/config.h"

#include "hkbhook/hkb-dll.h"

#include "platform/config.h"
#include "unityhook/config.h"

struct hkb_hook_config {
    struct platform_config platform;
    struct aime_config aime;
    struct dvd_config dvd;
    struct io4_config io4;
    struct vfd_config vfd;
    struct touch_screen_config touch;
    struct led15093_config led15093;
    struct unity_config unity;
    struct hkb_dll_config dll;
};

void hkb_dll_config_load(struct hkb_dll_config* cfg, const wchar_t* filename);

void hkb_hook_config_load(struct hkb_hook_config* cfg, const wchar_t* filename);