#pragma once

#include <stddef.h>

#include "amex/amex.h"
#include "board/sg-reader.h"
#include "board/config.h"
#include "board/led15093.h"
#include "gfxhook/gfx.h"

#include "sekitohook/sekito-dll.h"

#include "hooklib/config.h"
#include "hooklib/dvd.h"
#include "hooklib/printer_chc.h"

#include "platform/config.h"


struct sekito_hook_config {
    struct platform_config platform;
    struct aime_config aime;
    struct aime_config aime_queue;
    struct io4_config io4;
    struct dvd_config dvd;
    struct led15093_config led15093;
    struct y3_config y3;
    struct sekito_dll_config dll;
    struct printer_chc_config printer;
    struct gfx_config gfx;
    struct amex_config amex;
    struct amvideo_config amvideo;
};

void sekito_dll_config_load(
        struct sekito_dll_config *cfg,
        const wchar_t *filename);

void sekito_hook_config_load(
        struct sekito_hook_config *cfg,
        const wchar_t *filename);
