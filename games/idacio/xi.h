#pragma once

/* Can't call this xinput.h or it will conflict with <xinput.h> */

#include <windows.h>

#include "idacio/backend.h"
#include "idacio/config.h"

HRESULT idac_xi_init(const struct idac_xi_config *cfg, const struct idac_io_backend **backend);
