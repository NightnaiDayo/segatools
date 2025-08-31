#pragma once

#include <stddef.h>

#include "ekt-dll.h"
#include "amex/amex.h"

#include "board/sg-reader.h"
#include "board/config.h"
#include "board/led15093.h"

#include "hooklib/dvd.h"

#include "platform/config.h"
#include "unityhook/config.h"

struct y3_config {
    uint8_t enable;
    uint8_t ws_enable;
    uint16_t ws_port;
    uint32_t ws_timeout;
};

struct ekt_hook_config {
    struct platform_config platform;
    struct aime_config aime;
    struct io4_config io4;
    struct dvd_config dvd;
    struct led15093_config led15093;
    struct y3_config y3;
    struct ekt_dll_config dll;
    struct unity_config unity;
};

void ekt_hook_config_load(
        struct ekt_hook_config *cfg,
        const wchar_t *filename);

void y3_config_load(
        struct y3_config *cfg,
        const wchar_t *filename);
