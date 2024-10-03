#pragma once

#include <windows.h>

struct vfd_config {
    bool enable;
    int port;
    bool utf_conversion;
};


HRESULT vfd_hook_init(struct vfd_config *cfg, int default_port);

