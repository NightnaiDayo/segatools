#pragma once

#include <stddef.h>

#include "board/config.h"

#include "hooklib/dvd.h"

#include "mai2hook/mai2-dll.h"

#include "platform/config.h"

#include "mai2hook/touch.h"

#include "board/led15070.h"

#include "unityhook/config.h"

struct mai2_hook_config {
    struct platform_config platform;
    struct aime_config aime;
    struct dvd_config dvd;
    struct io4_config io4;
    struct vfd_config vfd;
    struct mai2_dll_config dll;
    struct touch_config touch;
    struct led15070_config led15070;
    struct unity_config unity;
};

void mai2_dll_config_load(
    struct mai2_dll_config *cfg,
    const wchar_t *filename);

void touch_config_load(
    struct touch_config *cfg,
    const wchar_t *filename);

void led15070_config_load(
    struct led15070_config *cfg,
    const wchar_t *filename);

void mai2_hook_config_load(
    struct mai2_hook_config *cfg,
    const wchar_t *filename);
