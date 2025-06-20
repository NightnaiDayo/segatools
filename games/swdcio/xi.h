#pragma once

/* Can't call this xinput.h or it will conflict with <xinput.h> */

#include <windows.h>

#include "swdcio/backend.h"
#include "swdcio/config.h"

HRESULT swdc_xi_init(const struct swdc_xi_config *cfg, const struct swdc_io_backend **backend);
