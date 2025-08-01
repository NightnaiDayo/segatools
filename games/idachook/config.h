#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "board/config.h"
#include "board/led15070.h"

#include "hooklib/dvd.h"

#include "idachook/idac-dll.h"
#include "idachook/indrun.h"

#include "platform/platform.h"

struct idac_hook_config {
    struct platform_config platform;
    struct aime_config aime;
    struct dvd_config dvd;
    struct io4_config io4;
    struct ffb_config ffb;
    struct idac_dll_config dll;
    struct led15070_config led15070;
    struct indrun_config indrun;
};

void idac_dll_config_load(
        struct idac_dll_config *cfg,
        const wchar_t *filename);

void idac_hook_config_load(
        struct idac_hook_config *cfg,
        const wchar_t *filename);

void indrun_config_load(
        struct indrun_config *cfg,
        const wchar_t *filename);
