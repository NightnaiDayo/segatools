#pragma once

#include "idacio/backend.h"
#include "idacio/config.h"

HRESULT idac_di_init(
        const struct idac_di_config *cfg,
        HINSTANCE inst,
        const struct idac_io_backend **backend);
