#pragma once

#include <windows.h>

struct unity_config {
    wchar_t target_assembly[MAX_PATH];
};

void unity_config_load(struct unity_config *cfg, const wchar_t *filename);
