#pragma once

#include <windows.h>

#include "ektio/backend.h"
#include "ektio/config.h"

HRESULT ekt_kb_init(const struct ekt_kb_config *cfg, const struct ekt_io_backend **backend);
