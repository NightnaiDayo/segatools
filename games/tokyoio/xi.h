#pragma once

#include <windows.h>

#include "tokyoio/backend.h"
#include "tokyoio/config.h"

HRESULT tokyo_xi_init(const struct tokyo_io_backend **backend);
