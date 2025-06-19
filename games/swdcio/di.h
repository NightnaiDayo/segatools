#pragma once

#include "swdcio/backend.h"
#include "swdcio/config.h"

HRESULT swdc_di_init(
        const struct swdc_di_config *cfg,
        HINSTANCE inst,
        const struct swdc_io_backend **backend);
