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
    HRESULT (*nfc_get_mifare_uid)(
            uint8_t unit_no,
            uint8_t *uid,
            size_t uid_size);
    HRESULT (*nfc_mifare_select)(
            uint8_t unit_no,
            const uint8_t *uid,
            size_t uid_size);
    HRESULT (*nfc_mifare_set_key)(
            uint8_t unit_no,
            uint8_t key_type,
            const uint8_t *key,
            size_t key_size);
    HRESULT (*nfc_mifare_authenticate)(
            uint8_t unit_no,
            uint8_t key_type,
            const uint8_t *payload,
            size_t payload_size);
    HRESULT (*nfc_mifare_read_block)(
            uint8_t unit_no,
            const uint8_t *uid,
            size_t uid_size,
            uint8_t block_no,
            uint8_t *block,
            size_t block_size);
    HRESULT (*nfc_felica_transact)(
            uint8_t unit_no,
            const uint8_t *req,
            size_t req_size,
            uint8_t *res,
            size_t res_size,
            size_t *res_size_written);
    HRESULT (*nfc_radio_on)(uint8_t unit_no);
    HRESULT (*nfc_radio_off)(uint8_t unit_no);
    HRESULT (*nfc_to_update_mode)(uint8_t unit_no);
    HRESULT (*nfc_send_hex_data)(
            uint8_t unit_no,
            const uint8_t *payload,
            size_t payload_size,
            uint8_t *status_out);
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
