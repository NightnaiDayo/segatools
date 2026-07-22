#pragma once

/* Can't call this xinput.h or it will conflict with <xinput.h> */

#include <windows.h>

#include "divaio/config.h"
#include "divaio/input/backend.h"

HRESULT diva_xi_init(const struct diva_xi_config *cfg,
                     const struct diva_io_backend **backend);
