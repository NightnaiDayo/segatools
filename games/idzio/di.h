#pragma once

#include "idzio/backend.h"
#include "idzio/config.h"

#define IDZ_DI_MAX_BUTTONS 128

HRESULT idz_di_init(
        const struct idz_di_config *cfg,
        HINSTANCE inst,
        const struct idz_io_backend **backend);
