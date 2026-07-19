#pragma once

#include <windows.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct nusec_config {
    bool enable;
    char keychip_id[16];
    char game_id[4];
    char platform_id[4];
    uint8_t region;
    uint8_t system_flag;
    uint32_t subnet;
    uint32_t bcast;
    uint16_t billing_type;
    wchar_t billing_ca[MAX_PATH];
    wchar_t billing_pub[MAX_PATH];
    bool persistence;
    wchar_t persistent_path[MAX_PATH];
};

struct nusec_log_record {
    uint8_t unknown[60];
};

struct nusec_save_data {
    uint32_t nearfull;
    uint32_t play_count;
    uint32_t play_limit;
    struct nusec_log_record log[7154];
    size_t log_head;
    size_t log_tail;
};

HRESULT nusec_hook_init(
        const struct nusec_config *cfg,
        const char *game_id,
        const char *platform_id);
