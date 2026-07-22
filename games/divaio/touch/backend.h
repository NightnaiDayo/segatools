#pragma once

#include "divaio/divaio.h"

struct diva_touch_backend {
    HRESULT (*init)();
    void (*update)(diva_io_touch_callback_t callback);
};
