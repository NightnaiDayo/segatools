#pragma once

#include <windows.h>

#include "apm3io/backend.h"
#include "apm3io/config.h"

HRESULT apm3_kb_init(const struct apm3_kb_config *cfg,
                     const struct apm3_io_backend **backend);
