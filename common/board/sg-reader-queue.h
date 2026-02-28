#pragma once

#include <windows.h>

#include <stdbool.h>

#include "board/aime-dll.h"
#include "board/sg-reader.h"

HRESULT sg_reader_queue_hook_init(
        const struct aime_config *cfg,
        unsigned int default_port_no,
        unsigned int gen,
        HINSTANCE self);
