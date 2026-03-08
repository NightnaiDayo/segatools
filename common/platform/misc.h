#pragma once

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

struct misc_config {
    bool enable;
    bool allowReboot;
    bool allowMasterKeyWrite;
};

HRESULT misc_hook_init(const struct misc_config *cfg, const char *platform_id);
