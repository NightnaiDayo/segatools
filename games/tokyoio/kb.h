#pragma once

#include <windows.h>

#include "tokyoio/backend.h"
#include "tokyoio/config.h"

HRESULT tokyo_kb_init(
    const struct tokyo_kb_config *cfg,
    const struct tokyo_io_backend **backend);
