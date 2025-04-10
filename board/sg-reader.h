#pragma once

#include <windows.h>

#include <stdbool.h>

#include "board/aime-dll.h"

struct aime_config {
    struct aime_dll_config dll;
    bool enable;
    unsigned int port_no;
    bool high_baudrate;
    unsigned int gen;
    unsigned int proxy_flag;
    wchar_t authdata_path[MAX_PATH];
};

HRESULT sg_reader_hook_init(
        const struct aime_config *cfg,
        unsigned int default_port_no,
        unsigned int gen,
        HINSTANCE self);
