#pragma once

#include <windows.h>

struct indrun_config {
    bool enable;
    wchar_t patch_file[MAX_PATH];
};

void indrun_hook_init(struct indrun_config *cfg);
