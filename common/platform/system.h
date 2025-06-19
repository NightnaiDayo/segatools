#pragma once

#include <windows.h>

#include <stdbool.h>
#include <stdint.h>

#include "platform/vfs.h"


struct dipsw_config
{
    wchar_t label[MAX_PATH];
    wchar_t on[MAX_PATH];
    wchar_t off[MAX_PATH];
};


struct system_config {
    bool enable;
    bool freeplay;
    bool dipsw[8];
    struct dipsw_config dipsw_config[8];
};

HRESULT system_init(const struct system_config *cfg, const struct vfs_config *vfs_cfg);
