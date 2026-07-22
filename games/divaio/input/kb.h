#pragma once

#include <windows.h>

#include "divaio/config.h"
#include "divaio/input/backend.h"

HRESULT diva_kb_init(const struct diva_kb_config *cfg,
                     const struct diva_io_backend **backend);
