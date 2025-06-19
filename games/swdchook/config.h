#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "board/config.h"
#include "board/led15070.h"

#include "hooklib/dvd.h"

#include "swdchook/swdc-dll.h"
#include "swdchook/zinput.h"

#include "platform/platform.h"

struct swdc_hook_config {
    struct platform_config platform;
    struct aime_config aime;
    struct dvd_config dvd;
    struct io4_config io4;
    struct ffb_config ffb;
    struct vfd_config vfd;
    struct led15070_config led15070;
    struct swdc_dll_config dll;
    struct zinput_config zinput;
};

void swdc_dll_config_load(
        struct swdc_dll_config *cfg,
        const wchar_t *filename);

void swdc_hook_config_load(
        struct swdc_hook_config *cfg,
        const wchar_t *filename);

void zinput_config_load(struct zinput_config *cfg, const wchar_t *filename);
