#pragma once

#include <windows.h>

#include <stdbool.h>
#include <stddef.h>

bool io_path_config_load(
        wchar_t *out,
        size_t out_count,
        const wchar_t *section,
        const wchar_t *key,
        const wchar_t *env_name,
        const wchar_t *root_filename,
        const wchar_t *filename);
