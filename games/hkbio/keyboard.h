#pragma once

#include <windows.h>

#include "hkbio/backend.h"
#include "hkbio/config.h"

HRESULT hkb_kb_init(const struct hkb_kb_config* cfg, const struct hkb_io_backend** backend);
