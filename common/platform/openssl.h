#pragma once

#include <windows.h>

#include <stdbool.h>

struct openssl_config {
    bool enable;
    bool override;
};

HRESULT openssl_hook_init(const struct openssl_config *cfg);
