#pragma once

#include <windows.h>

#include "board/io4.h"

HRESULT hkb_io4_hook_init(const struct io4_config* cfg, bool is_terminal);