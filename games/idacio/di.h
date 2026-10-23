#pragma once

#include "idacio/backend.h"
#include "idacio/config.h"

#define IDAC_DI_MAX_BUTTONS 128

HRESULT idac_di_init(
        const struct idac_di_config *cfg,
        HINSTANCE inst,
        const struct idac_io_backend **backend);
