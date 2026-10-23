#pragma once

#include "divaio/config.h"
#include "divaio/input/backend.h"

#define DIVA_DI_MAX_BUTTONS 128

HRESULT diva_di_init(const struct diva_di_config *cfg, HINSTANCE inst,
                     const struct diva_io_backend **backend);
