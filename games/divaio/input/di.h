#pragma once

#include "divaio/config.h"
#include "divaio/input/backend.h"

HRESULT diva_di_init(const struct diva_di_config *cfg, HINSTANCE inst,
                     const struct diva_io_backend **backend);
