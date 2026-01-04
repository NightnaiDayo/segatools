#pragma once

#include <windows.h>
#include <stdbool.h>

#include "aimeio/aimeio.h"

struct aime_dll {
    uint16_t api_version;
    HRESULT (*init)(void);
    HRESULT (*nfc_poll)(uint8_t unit_no);
    HRESULT (*nfc_get_aime_id)(
            uint8_t unit_no,
            uint8_t *luid,
            size_t luid_size);
    HRESULT (*nfc_get_felica_id)(uint8_t unit_no, uint64_t *IDm);
    void (*led_set_color)(uint8_t unit_no, uint8_t r, uint8_t g, uint8_t b);
    void (*vfd_set_text)(
            const uint8_t *text,
            size_t text_len,
            const struct aime_io_vfd_state *state);
    void (*vfd_set_state)(const struct aime_io_vfd_state *state);
};

struct aime_dll_config {
    wchar_t path[MAX_PATH];
    bool path64;
};

extern struct aime_dll aime_dll;

HRESULT aime_dll_init(const struct aime_dll_config *cfg, HINSTANCE self);
