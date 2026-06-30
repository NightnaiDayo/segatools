#pragma once

#include <windows.h>

#include <stdint.h>
#include <stdbool.h>

#define LAYEREDFS_VERBOSITY_REDIRECTS 1
#define LAYEREDFS_VERBOSITY_CANDIDATES 2
#define LAYEREDFS_VERBOSITY_ALL_OPENS 3

struct layeredfs_config {
    bool enable;
    uint8_t verbosity;
    wchar_t rootDirectory[MAX_PATH];
    wchar_t modDirectory[MAX_PATH];
    bool allowAmdaemon;
};

HRESULT layeredfs_hook_init(const struct layeredfs_config* config);