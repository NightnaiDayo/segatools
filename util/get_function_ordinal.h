#pragma once

#include <dbghelp.h>
#include "dprintf.h"

DWORD get_function_ordinal(const char* dllName, const char* functionName);