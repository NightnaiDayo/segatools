#pragma once

#include "hooklib/touch.h"

void amptw_hook_init(const struct touch_screen_config* cfg, HMODULE hmodule);
void amptw_apply_hooks(HMODULE module);
