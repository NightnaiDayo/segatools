#pragma once

#include <stddef.h>

#include "board/config.h"
#include "board/led15093.h"

#include "hooklib/dvd.h"
#include "hooklib/touch.h"
#include "hooklib/printer.h"

#include "gfxhook/config.h"

#include "fgohook/deck.h"
#include "fgohook/ftdi.h"
#include "fgohook/fgo-dll.h"

#include "platform/config.h"

struct fgo_hook_config {
    struct platform_config platform;
    struct aime_config aime;
    struct dvd_config dvd;
    struct io4_config io4;
    struct vfd_config vfd;
    struct touch_screen_config touch;
    struct printer_config printer;
    struct deck_config deck;
    struct ftdi_config ftdi;
    struct led15093_config led15093;
    struct gfx_config gfx;
    struct fgo_dll_config dll;
};

void fgo_dll_config_load(
        struct fgo_dll_config *cfg,
        const wchar_t *filename);

void fgo_hook_config_load(
        struct fgo_hook_config *cfg,
        const wchar_t *filename);
