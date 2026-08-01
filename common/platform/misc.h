#pragma once

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

struct misc_config {
    bool enable;
    bool allowReboot;
    bool allowMasterKeyWrite;
    wchar_t nextProcessFile[MAX_PATH];
};

HRESULT misc_hook_init(const struct misc_config *cfg, const char *platform_id);
