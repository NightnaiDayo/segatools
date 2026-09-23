#pragma once

#include <windows.h>

#include <stdbool.h>

#define REGISTRY_STORE_PROFILE_MAX 32

enum registry_path_error {
    REGISTRY_PATH_OK,
    REGISTRY_PATH_TOO_LONG,
    REGISTRY_PATH_NOT_LOCAL,
    REGISTRY_PATH_BAD_NAME,
};

struct registry_config {
    enum registry_path_error path_error;
    bool profile_truncated;
    wchar_t path[MAX_PATH];
    wchar_t profile[REGISTRY_STORE_PROFILE_MAX + 1];
};

/* Install registry hooks separately in DLLs loaded later. */
HRESULT registry_store_push_redirect(
        const struct registry_config *cfg,
        const char *default_profile,
        HKEY from_root,
        const wchar_t *from_prefix);
