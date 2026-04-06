#pragma once

#include <windows.h>

#include <stdbool.h>

struct elo_config {
    bool enable;
    unsigned int port_no;
};

HRESULT elo_hook_init(const struct elo_config *cfg, unsigned int port_no);
