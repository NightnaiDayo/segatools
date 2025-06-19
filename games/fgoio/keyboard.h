#pragma once

#include <windows.h>

#include "fgoio/backend.h"
#include "fgoio/config.h"

HRESULT fgo_kb_init(const struct fgo_kb_config *cfg, const struct fgo_io_backend **backend);
