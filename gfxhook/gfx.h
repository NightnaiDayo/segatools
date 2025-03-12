#pragma once

#include <stdbool.h>

struct gfx_config {
    bool enable;
    bool windowed;
    bool framed;
    int monitor;
    bool dpiAware;
};

void gfx_hook_init(const struct gfx_config *cfg);
