#pragma once

#include <windows.h>

#include "config.h"

typedef void (*unity_hook_callback_func)(HMODULE, const wchar_t*);

void unity_hook_init(const struct unity_config *cfg, HINSTANCE self, unity_hook_callback_func callback);
