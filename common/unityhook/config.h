#pragma once

#include <stdbool.h>

#include <windows.h>

struct unity_config {
    bool enable;
    bool debug_enable;
    bool debug_suspend;
    wchar_t target_assembly[MAX_PATH];
    wchar_t debug_address[MAX_PATH];
};

void unity_config_load(struct unity_config *cfg, const wchar_t *filename);
