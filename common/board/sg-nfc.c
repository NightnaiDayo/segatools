#include <windows.h>

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "board/sg-cmd.h"
#include "board/sg-nfc.h"
#include "board/sg-nfc-cmd.h"

#include "aimeio/aimeio.h"

#include "iccard/aime.h"
#include "iccard/felica.h"

#include "util/dprintf.h"
#include "util/dump.h"
#include "util/slurp.h"

static HRESULT sg_nfc_dispatch(
        void *ctx,
        const void *v_req,
        void *v_res);

static HRESULT sg_nfc_cmd_reset(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res);

static HRESULT sg_nfc_cmd_get_fw_version(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_nfc_res_get_fw_version *res);

static HRESULT sg_nfc_cmd_get_hw_version(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_nfc_res_get_hw_version *res);

static HRESULT sg_nfc_cmd_poll(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_nfc_res_poll *res);

static HRESULT sg_nfc_poll_aime(
        struct sg_nfc *nfc,
        struct sg_nfc_poll_mifare *mifare);

static HRESULT sg_nfc_poll_felica(
        struct sg_nfc *nfc,
        struct sg_nfc_poll_felica *felica);

static HRESULT sg_nfc_cmd_mifare_read_block(
        struct sg_nfc *nfc,
        const struct sg_nfc_req_mifare_read_block *req,
        struct sg_nfc_res_mifare_read_block *res);

static HRESULT sg_nfc_cmd_mifare_select(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res);

static HRESULT sg_nfc_cmd_mifare_set_key(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res,
        uint8_t key_type);

static HRESULT sg_nfc_cmd_mifare_authenticate(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res,
        uint8_t key_type);

static HRESULT sg_nfc_cmd_radio_on(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res);

static HRESULT sg_nfc_cmd_radio_off(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res);

static HRESULT sg_nfc_cmd_to_update_mode(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res);

static HRESULT sg_nfc_cmd_felica_encap(
        struct sg_nfc *nfc,
        const struct sg_nfc_req_felica_encap *req,
        struct sg_nfc_res_felica_encap *res);

static HRESULT sg_nfc_cmd_send_hex_data(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res);

static HRESULT sg_nfc_cmd_dummy(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res);

static const struct version_info hw_version[] = {
    {"TN32MSEC003S H/W Ver3.0", 23},
    {"837-15286", 9},
    {"837-15396", 9}
};

static const struct version_info fw_version[] = {
    {"TN32MSEC003S F/W Ver1.2", 23},
    {"\x94", 1},
    {"\x94", 1}
};

void sg_nfc_init(
        struct sg_nfc *nfc,
        uint8_t addr,
        const struct sg_nfc_ops *ops,
        unsigned int gen,
        unsigned int proxy_flag,
        const wchar_t* authdata_path,
        bool mobile_felica,
        void *ops_ctx)
{
    assert(nfc != NULL);
    assert(ops != NULL);

    nfc->ops = ops;
    nfc->ops_ctx = ops_ctx;
    nfc->addr = addr;
    nfc->gen = gen;
    nfc->proxy_flag = proxy_flag;
    nfc->authdata_path = authdata_path;
    nfc->felica.is_mobile = mobile_felica;
}

#ifdef NDEBUG
#define sg_nfc_dprintfv(nfc, fmt, ap)
#define sg_nfc_dprintf(nfc, fmt, ...)
#else
static void sg_nfc_dprintfv(
        const struct sg_nfc *nfc,
        const char *fmt,
        va_list ap)
{
    dprintf("NFC %02x: ", nfc->addr);
    dprintfv(fmt, ap);
}

static void sg_nfc_dprintf(
        const struct sg_nfc *nfc,
        const char *fmt,
        ...)
{
    va_list ap;

    va_start(ap, fmt);
    sg_nfc_dprintfv(nfc, fmt, ap);
    va_end(ap);
}
#endif

void sg_nfc_transact(
        struct sg_nfc *nfc,
        struct iobuf *res_frame,
        const void *req_bytes,
        size_t req_nbytes)
{
    assert(nfc != NULL);
    assert(res_frame != NULL);
    assert(req_bytes != NULL);

    sg_req_transact(res_frame, req_bytes, req_nbytes, sg_nfc_dispatch, nfc);
}

static HRESULT sg_nfc_dispatch(
        void *ctx,
        const void *v_req,
        void *v_res)
{
    struct sg_nfc *nfc;
    const union sg_nfc_req_any *req;
    union sg_nfc_res_any *res;

    nfc = ctx;
    req = v_req;
    res = v_res;

    if (req->simple.hdr.addr != nfc->addr) {
        /* Not addressed to us, don't send a response */
        return S_FALSE;
    }

    switch (req->simple.hdr.cmd) {
    case SG_NFC_CMD_RESET:
        return sg_nfc_cmd_reset(nfc, &req->simple, &res->simple);

    case SG_NFC_CMD_GET_FW_VERSION:
        return sg_nfc_cmd_get_fw_version(
                nfc,
                &req->simple,
                &res->get_fw_version);

    case SG_NFC_CMD_GET_HW_VERSION:
        return sg_nfc_cmd_get_hw_version(
                nfc,
                &req->simple,
                &res->get_hw_version);

    case SG_NFC_CMD_POLL:
        return sg_nfc_cmd_poll(
                nfc,
                &req->simple,
                &res->poll);

    case SG_NFC_CMD_MIFARE_READ_BLOCK:
        return sg_nfc_cmd_mifare_read_block(
                nfc,
                &req->mifare_read_block,
                &res->mifare_read_block);

    case SG_NFC_CMD_FELICA_ENCAP:
        return sg_nfc_cmd_felica_encap(
                nfc,
                &req->felica_encap,
                &res->felica_encap);

    case SG_NFC_CMD_MIFARE_SELECT_TAG:
        return sg_nfc_cmd_mifare_select(nfc, &req->simple, &res->simple);

    case SG_NFC_CMD_MIFARE_SET_KEY_AIME:
        return sg_nfc_cmd_mifare_set_key(
                nfc,
                &req->simple,
                &res->simple,
                AIME_IO_MIFARE_KEY_AIME);

    case SG_NFC_CMD_MIFARE_SET_KEY_BANA:
        return sg_nfc_cmd_mifare_set_key(
                nfc,
                &req->simple,
                &res->simple,
                AIME_IO_MIFARE_KEY_BANA);

    case SG_NFC_CMD_MIFARE_AUTHENTICATE_AIME:
        return sg_nfc_cmd_mifare_authenticate(
                nfc,
                &req->simple,
                &res->simple,
                AIME_IO_MIFARE_KEY_AIME);

    case SG_NFC_CMD_MIFARE_AUTHENTICATE_BANA:
        return sg_nfc_cmd_mifare_authenticate(
                nfc,
                &req->simple,
                &res->simple,
                AIME_IO_MIFARE_KEY_BANA);

    case SG_NFC_CMD_RADIO_ON:
        return sg_nfc_cmd_radio_on(nfc, &req->simple, &res->simple);

    case SG_NFC_CMD_RADIO_OFF:
        return sg_nfc_cmd_radio_off(nfc, &req->simple, &res->simple);

    case SG_NFC_CMD_TO_UPDATE_MODE:
        return sg_nfc_cmd_to_update_mode(nfc, &req->simple, &res->simple);

    case SG_NFC_CMD_SEND_HEX_DATA:
        return sg_nfc_cmd_send_hex_data(nfc, &req->simple, &res->simple);

    default:
        sg_nfc_dprintf(nfc, "Unimpl command %02x\n", req->simple.hdr.cmd);

        return E_NOTIMPL;
    }
}

static HRESULT sg_nfc_cmd_reset(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res)
{
    sg_nfc_dprintf(nfc, "Reset\n");
    sg_res_init(res, req, 0);
    res->status = 3;

    return S_OK;
}

static HRESULT sg_nfc_cmd_get_fw_version(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_nfc_res_get_fw_version *res)
{
    const struct version_info *fw = &fw_version[nfc->gen - 1];

    /* Dest version is not NUL terminated, this is intentional */
    sg_res_init(&res->res, req, fw->length);
    memcpy(res->version, fw->version, fw->length);

    return S_OK;
}

static HRESULT sg_nfc_cmd_get_hw_version(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_nfc_res_get_hw_version *res)
{
    const struct version_info *hw = &hw_version[nfc->gen - 1];

    /* Dest version is not NUL terminated, this is intentional */
    sg_res_init(&res->res, req, hw->length);
    memcpy(res->version, hw->version, hw->length);

    return S_OK;
}

static HRESULT sg_nfc_cmd_poll(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_nfc_res_poll *res)
{
    struct sg_nfc_poll_mifare mifare;
    struct sg_nfc_poll_felica felica;
    HRESULT hr;

    hr = nfc->ops->poll(nfc->ops_ctx);

    if (FAILED(hr)) {
        return hr;
    }

    hr = sg_nfc_poll_felica(nfc, &felica);

    if (SUCCEEDED(hr) && hr != S_FALSE) {
        sg_res_init(&res->res, req, 1 + sizeof(felica));
        memcpy(res->payload, &felica, sizeof(felica));
        res->count = 1;

        return S_OK;
    }

    hr = sg_nfc_poll_aime(nfc, &mifare);

    if (SUCCEEDED(hr) && hr != S_FALSE) {
        sg_res_init(&res->res, req, 1 + sizeof(mifare));
        memcpy(res->payload, &mifare, sizeof(mifare));
        res->count = 1;

        return S_OK;
    }

    sg_res_init(&res->res, req, 1);
    res->count = 0;

    return S_OK;
}

static HRESULT sg_nfc_poll_aime(
        struct sg_nfc *nfc,
        struct sg_nfc_poll_mifare *mifare)
{
    bool has_uid;
    uint8_t luid[10];
    uint8_t uid[4];
    HRESULT hr;

    /* Call backend */

    if (nfc->ops->get_aime_id != NULL) {
        hr = nfc->ops->get_aime_id(nfc->ops_ctx, luid, sizeof(luid));
    } else {
        hr = S_FALSE;
    }

    if (FAILED(hr) || hr == S_FALSE) {
        return hr;
    }

    sg_nfc_dprintf(nfc, "AiMe card is present\n");

    has_uid = false;

    if (nfc->ops->get_mifare_uid != NULL) {
        hr = nfc->ops->get_mifare_uid(nfc->ops_ctx, uid, sizeof(uid));

        if (FAILED(hr)) {
            return hr;
        }

        if (hr == S_OK) {
            has_uid = true;
        }
    }

    /* Construct response */

    mifare->type = 0x10;
    mifare->id_len = sizeof(mifare->uid);
    if (has_uid) {
        memcpy(&mifare->uid, uid, sizeof(uid));
    } else {
        // mifare->uid = _byteswap_ulong(0x8FBECBFF);
        mifare->uid = _byteswap_ulong(0x01020304);
    }

    /* Initialize MIFARE IC emulator */

    hr = aime_card_populate(&nfc->mifare, luid, sizeof(luid));

    if (FAILED(hr)) {
        return hr;
    }

    return S_OK;
}

static HRESULT sg_nfc_poll_felica(
        struct sg_nfc *nfc,
        struct sg_nfc_poll_felica *felica)
{
    uint64_t IDm;
    HRESULT hr;

    /* Call backend */

    if (nfc->ops->get_felica_id != NULL) {
        hr = nfc->ops->get_felica_id(nfc->ops_ctx, &IDm);
    } else {
        hr = S_FALSE;
    }

    if (FAILED(hr) || hr == S_FALSE) {
        return hr;
    }

    sg_nfc_dprintf(nfc, "FeliCa card is present\n");

    /* Construct poll response */

    felica->type = 0x20;
    felica->id_len = sizeof(felica->IDm) + sizeof(felica->PMm);
    felica->IDm = _byteswap_uint64(IDm);
    felica->PMm = _byteswap_uint64(felica_get_amusement_ic_PMm(nfc->felica.is_mobile));

    /* Initialize FeliCa IC emulator */

    nfc->felica.IDm = IDm;
    nfc->felica.PMm = felica_get_amusement_ic_PMm(nfc->felica.is_mobile);
    nfc->felica.system_code = 0x88b4;

    return S_OK;
}

static HRESULT sg_nfc_cmd_mifare_select(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res)
{
    const uint8_t *payload;
    HRESULT hr;

    if (req->payload_len != sizeof(uint32_t)) {
        sg_nfc_dprintf(nfc, "%s: Payload size is incorrect\n", __func__);

        return E_FAIL;
    }

    payload = (const uint8_t *) req + sizeof(*req);

    if (nfc->ops->mifare_select != NULL) {
        hr = nfc->ops->mifare_select(nfc->ops_ctx, payload, req->payload_len);

        if (FAILED(hr)) {
            return hr;
        }
    }

    sg_res_init(res, req, 0);

    return S_OK;
}

static HRESULT sg_nfc_cmd_mifare_set_key(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res,
        uint8_t key_type)
{
    const uint8_t *payload;
    HRESULT hr;

    payload = (const uint8_t *) req + sizeof(*req);

    if (nfc->ops->mifare_set_key != NULL) {
        hr = nfc->ops->mifare_set_key(
                nfc->ops_ctx,
                key_type,
                payload,
                req->payload_len);

        if (FAILED(hr)) {
            return hr;
        }
    }

    sg_res_init(res, req, 0);

    return S_OK;
}

static HRESULT sg_nfc_cmd_mifare_authenticate(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res,
        uint8_t key_type)
{
    const uint8_t *payload;
    HRESULT hr;

    payload = (const uint8_t *) req + sizeof(*req);

    if (nfc->ops->mifare_authenticate != NULL) {
        hr = nfc->ops->mifare_authenticate(
                nfc->ops_ctx,
                key_type,
                payload,
                req->payload_len);

        if (FAILED(hr)) {
            return hr;
        }
    }

    sg_res_init(res, req, 0);

    return S_OK;
}

static HRESULT sg_nfc_cmd_radio_on(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res)
{
    HRESULT hr;

    if (nfc->ops->radio_on == NULL) {
        sg_res_init(res, req, 0);
        return S_OK;
    }

    hr = nfc->ops->radio_on(nfc->ops_ctx);

    if (FAILED(hr)) {
        return hr;
    }

    if (hr == S_FALSE) {
        sg_res_init(res, req, 0);
        return S_OK;
    }

    sg_res_init(res, req, 0);

    return S_OK;
}

static HRESULT sg_nfc_cmd_radio_off(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res)
{
    HRESULT hr;

    if (nfc->ops->radio_off == NULL) {
        sg_res_init(res, req, 0);
        return S_OK;
    }

    hr = nfc->ops->radio_off(nfc->ops_ctx);

    if (FAILED(hr)) {
        return hr;
    }

    if (hr == S_FALSE) {
        sg_res_init(res, req, 0);
        return S_OK;
    }

    sg_res_init(res, req, 0);

    return S_OK;
}

static HRESULT sg_nfc_cmd_to_update_mode(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res)
{
    HRESULT hr;

    if (nfc->ops->to_update_mode == NULL) {
        sg_res_init(res, req, 0);
        return S_OK;
    }

    hr = nfc->ops->to_update_mode(nfc->ops_ctx);

    if (FAILED(hr)) {
        return hr;
    }

    if (hr == S_FALSE) {
        sg_res_init(res, req, 0);
        return S_OK;
    }

    sg_res_init(res, req, 0);

    return S_OK;
}

static HRESULT sg_nfc_cmd_mifare_read_block(
        struct sg_nfc *nfc,
        const struct sg_nfc_req_mifare_read_block *req,
        struct sg_nfc_res_mifare_read_block *res)
{
    const uint8_t *uid_bytes;
    HRESULT hr;
    uint32_t uid;

    if (req->req.payload_len != sizeof(req->payload)) {
        sg_nfc_dprintf(nfc, "%s: Payload size is incorrect\n", __func__);

        return E_FAIL;
    }

    uid_bytes = (const uint8_t *) &req->payload.uid;
    uid = _byteswap_ulong(req->payload.uid);

    sg_nfc_dprintf(nfc, "Read uid %08x block %i\n", uid, req->payload.block_no);

    if (nfc->ops->mifare_read_block != NULL) {
        hr = nfc->ops->mifare_read_block(
                nfc->ops_ctx,
                uid_bytes,
                sizeof(req->payload.uid),
                req->payload.block_no,
                res->block,
                sizeof(res->block));

        if (FAILED(hr)) {
            return hr;
        }

        if (hr == S_OK) {
            sg_res_init(&res->res, &req->req, sizeof(res->block));
            return S_OK;
        }
    }

    if (req->payload.block_no > 14) {
        sg_nfc_dprintf(nfc, "MIFARE block number out of range\n");

        return E_FAIL;
    } else if (req->payload.block_no >= 5){ // emoney auth encrypted

        sg_res_init(&res->res, &req->req, sizeof(res->block));

        char* auth;
        long size = wslurp(nfc->authdata_path, &auth, false);
        if (size < 0){
            sg_nfc_dprintf(nfc, "Failed to read %ls: %lx!\n", nfc->authdata_path, GetLastError());
            return E_FAIL;
        }

        int offset = 0;
        if (req->payload.block_no == 6){
            offset = 16;
        } else if (req->payload.block_no == 8){
            offset = 32;
        } else if (req->payload.block_no == 9){
            offset = 48;
        } else if (req->payload.block_no == 10){
            offset = 64;
        } else if (req->payload.block_no == 12){
            offset = 82;
        } else if (req->payload.block_no == 13){
            offset = 98;
        } else if (req->payload.block_no == 14){
            offset = 114;
        }

        for (int i = 0; i < 16 && offset + i < size; i++){
            res->block[i] = auth[offset + i];
        }

        free(auth);

    } else if (req->payload.block_no == 4){ // emoney auth plain

        sg_res_init(&res->res, &req->req, sizeof(res->block));

        res->block[0] = 0x54; // header
        res->block[1] = 0x43;
        res->block[2] = nfc->proxy_flag; // 2 or 3 depending on game (useProxy in env.json)
        res->block[3] = 0x01; // unknown flag

    } else { // read all other blocks normally

        sg_res_init(&res->res, &req->req, sizeof(res->block));

        memcpy( res->block,
                nfc->mifare.sectors[0].blocks[req->payload.block_no].bytes,
                sizeof(res->block));
    }

    return S_OK;
}

static HRESULT sg_nfc_cmd_felica_encap(
        struct sg_nfc *nfc,
        const struct sg_nfc_req_felica_encap *req,
        struct sg_nfc_res_felica_encap *res)
{
    struct const_iobuf f_req;
    struct iobuf f_res;
    size_t res_size_written;
    HRESULT hr;

    /* First byte of encapsulated request and response is a length byte
       (inclusive of itself). The FeliCa emulator expects its caller to handle
       that length byte on its behalf (we adopt the convention that the length
       byte is part of the FeliCa protocol's framing layer). */

    if (req->req.payload_len != 8 + req->payload[0]) {
        sg_nfc_dprintf(
                nfc,
                "FeliCa encap payload length mismatch: sg %i != felica %i + 8",
                req->req.payload_len,
                req->payload[0]);

        return E_FAIL;
    }

    if (nfc->ops->felica_transact != NULL) {
        res_size_written = 0;
        hr = nfc->ops->felica_transact(
                nfc->ops_ctx,
                req->payload,
                req->payload[0],
                res->payload,
                sizeof(res->payload),
                &res_size_written);

        if (FAILED(hr)) {
            return hr;
        }

        if (hr == S_OK) {
            if (res_size_written == 0 ||
                    res_size_written > sizeof(res->payload)) {
                return E_FAIL;
            }

            sg_res_init(&res->res, &req->req, res_size_written);

            return S_OK;
        }
    }

    f_req.bytes = req->payload;
    f_req.nbytes = req->payload[0];
    f_req.pos = 1;

    f_res.bytes = res->payload;
    f_res.nbytes = sizeof(res->payload);
    f_res.pos = 1;

#if defined(LOG_NFC)
    dprintf("FELICA OUTBOUND:\n");
    dump_const_iobuf(&f_req);
#endif

    hr = felica_transact(&nfc->felica, &f_req, &f_res);

    if (FAILED(hr)) {
        return hr;
    }

    sg_res_init(&res->res, &req->req, f_res.pos);
    res->payload[0] = f_res.pos;

#if defined(LOG_NFC)
    dprintf("FELICA INBOUND:\n");
    dump_iobuf(&f_res);
#endif

    return S_OK;
}

static HRESULT sg_nfc_cmd_send_hex_data(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res)
{
    const uint8_t *payload;
    HRESULT hr;
    uint8_t status;

    if (nfc->ops->send_hex_data == NULL) {
        sg_res_init(res, req, 0);

        /* Firmware checksum length? */
        if (req->payload_len == 0x2b) {
             /* The firmware is identical flag? */
            res->status = 0x20;
        }

        return S_OK;
    }

    payload = (const uint8_t *) req + sizeof(*req);
    status = 0;

    hr = nfc->ops->send_hex_data(
            nfc->ops_ctx,
            payload,
            req->payload_len,
            &status);

    if (FAILED(hr)) {
        return hr;
    }

    if (hr == S_FALSE) {
        sg_res_init(res, req, 0);

        if (req->payload_len == 0x2b) {
            res->status = 0x20;
        }

        return S_OK;
    }

    sg_res_init(res, req, 0);
    res->status = status;

    return S_OK;
}

static HRESULT sg_nfc_cmd_dummy(
        struct sg_nfc *nfc,
        const struct sg_req_header *req,
        struct sg_res_header *res)
{
    sg_res_init(res, req, 0);

    return S_OK;
}
