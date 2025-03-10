#pragma once

/* Can't call this xinput.h or it will conflict with <xinput.h> */

#include <windows.h>

#include "fgoio/backend.h"
#include "fgoio/config.h"

HRESULT fgo_xi_init(const struct fgo_xi_config *cfg, const struct fgo_io_backend **backend);
