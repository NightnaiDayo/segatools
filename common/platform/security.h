#pragma once

#include <windows.h>

#include <stdbool.h>
#include <stddef.h>

HRESULT security_hook_init();
void security_hook_insert_hooks(HMODULE mod);
