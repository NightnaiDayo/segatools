#pragma once

#include <stddef.h>

#include "board/config.h"
#include "board/led15093.h"

#include "hooklib/dvd.h"

#include "tokyohook/tokyo-dll.h"
#include "tokyohook/zinput.h"

#include "platform/config.h"

struct tokyo_hook_config {
    struct platform_config platform;
    struct dvd_config dvd;
    struct io4_config io4;
    struct led15093_config led15093;
    struct zinput_config zinput;
    struct tokyo_dll_config dll;
};

void tokyo_dll_config_load(
        struct tokyo_dll_config *cfg,
        const wchar_t *filename);

void tokyo_hook_config_load(
        struct tokyo_hook_config *cfg,
        const wchar_t *filename);
