#pragma once

#include "swdcio/backend.h"
#include "swdcio/config.h"

#define SWDC_DI_MAX_BUTTONS 128

HRESULT swdc_di_init(
        const struct swdc_di_config *cfg,
        HINSTANCE inst,
        const struct swdc_io_backend **backend);
