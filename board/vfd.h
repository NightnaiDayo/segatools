#pragma once

#include <windows.h>

struct vfd_config {
    bool enable;
    unsigned int port_no;
    bool utf_conversion;
};


HRESULT vfd_hook_init(struct vfd_config *cfg, unsigned int default_port_no);

