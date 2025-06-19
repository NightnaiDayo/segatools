#pragma once

#include <windows.h>

struct indrun_config {
    bool enable;
};

void indrun_hook_init(struct indrun_config *cfg);
