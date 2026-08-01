#pragma once

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include "hook/iobuf.h"

#include "iccard/felica.h"
#include "iccard/mifare.h"

struct sg_nfc_ops {
    HRESULT (*poll)(void *ctx);
    HRESULT (*get_aime_id)(void *ctx, uint8_t *luid, size_t nbytes);
    HRESULT (*get_felica_id)(void *ctx, uint64_t *IDm);
    HRESULT (*get_mifare_uid)(void *ctx, uint8_t *uid, size_t nbytes);
    HRESULT (*mifare_select)(void *ctx, const uint8_t *uid, size_t nbytes);
    HRESULT (*mifare_set_key)(
            void *ctx,
            uint8_t key_type,
            const uint8_t *key,
            size_t nbytes);
    HRESULT (*mifare_authenticate)(
            void *ctx,
            uint8_t key_type,
            const uint8_t *payload,
            size_t nbytes);
    HRESULT (*mifare_read_block)(
            void *ctx,
            const uint8_t *uid,
            size_t uid_size,
            uint8_t block_no,
            uint8_t *block,
            size_t block_size);
    HRESULT (*felica_transact)(
            void *ctx,
            const uint8_t *req,
            size_t req_size,
            uint8_t *res,
            size_t res_size,
            size_t *res_size_written);
    HRESULT (*radio_on)(void *ctx);
    HRESULT (*radio_off)(void *ctx);
    HRESULT (*to_update_mode)(void *ctx);
    HRESULT (*send_hex_data)(
            void *ctx,
            const uint8_t *payload,
            size_t payload_size,
            uint8_t *status_out);

    // TODO Banapass, AmuseIC
};

struct sg_nfc {
    const struct sg_nfc_ops *ops;
    void *ops_ctx;
    uint8_t addr;
    unsigned int gen;
    unsigned int proxy_flag;
    struct felica felica;
    struct mifare mifare;
    const wchar_t* authdata_path;
};

void sg_nfc_init(
        struct sg_nfc *nfc,
        uint8_t addr,
        const struct sg_nfc_ops *ops,
        unsigned int gen,
        unsigned int proxy_flag,
        const wchar_t* authdata_path,
        bool mobile_felica,
        void *ops_ctx);

void sg_nfc_transact(
        struct sg_nfc *nfc,
        struct iobuf *res_frame,
        const void *req_bytes,
        size_t req_nbytes);
