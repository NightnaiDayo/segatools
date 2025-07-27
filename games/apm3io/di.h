#pragma once

#include "apm3io/backend.h"
#include "apm3io/config.h"

HRESULT apm3_di_init(const struct apm3_di_config *cfg, HINSTANCE inst,
                     const struct apm3_io_backend **backend);
