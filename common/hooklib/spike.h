#pragma once

#include <stddef.h>

void spike_hook_init(const wchar_t *ini_file);
void spike_hook_read_config(const wchar_t *target, const wchar_t *spike_file);
