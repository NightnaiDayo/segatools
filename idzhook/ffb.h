#pragma once

#include <windows.h>

#include "board/ffb.h"

HRESULT idz_ffb_hook_init(const struct ffb_config *cfg, unsigned int port_no);
