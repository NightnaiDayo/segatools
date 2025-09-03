#pragma once

#include <stddef.h>

#include "amex/amex.h"

#include "board/config.h"
#include "board/led15093.h"

#include "hooklib/dvd.h"

#include "kemonohook/kemono-dll.h"

#include "platform/config.h"

#include "unityhook/config.h"

struct kemono_hook_config {
    struct platform_config platform;
    struct aime_config aime;
    struct dvd_config dvd;
    struct vfd_config vfd;
    struct kemono_dll_config dll;
    struct unity_config unity;
    struct printer_chc_config printer;
    struct amex_config amex;
    struct led15093_config led15093;
};

void kemono_dll_config_load(
        struct kemono_dll_config *cfg,
        const wchar_t *filename);

void kemono_hook_config_load(
        struct kemono_hook_config *cfg,
        const wchar_t *filename);
