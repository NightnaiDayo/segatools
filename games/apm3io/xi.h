#pragma once

/* Can't call this xinput.h or it will conflict with <xinput.h> */

#include <windows.h>

#include "apm3io/backend.h"
#include "apm3io/config.h"

HRESULT apm3_xi_init(const struct apm3_xi_config *cfg,
                     const struct apm3_io_backend **backend);
