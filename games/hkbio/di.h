#pragma once

#include "hkbio/backend.h"
#include "hkbio/config.h"

HRESULT hkb_di_init(const struct hkb_di_config* cfg, HINSTANCE inst, const struct hkb_io_backend** backend);
