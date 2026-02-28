#pragma once

#include <windows.h>

#include "sekitoio/backend.h"
#include "sekitoio/config.h"

HRESULT sekito_kb_init(const struct sekito_kb_config *cfg, const struct sekito_io_backend **backend);
