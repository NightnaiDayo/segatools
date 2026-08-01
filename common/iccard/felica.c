#include <windows.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "hook/iobuf.h"

#include "iccard/felica.h"

#include "util/dprintf.h"
#include "util/dump.h"

static HRESULT felica_cmd_poll(
        struct felica *f,
        struct const_iobuf *req,
        struct iobuf *res);

static HRESULT felica_cmd_get_system_code(
        struct felica *f,
        struct const_iobuf *req,
        struct iobuf *res);

static HRESULT felica_cmd_active(
        struct felica *f,
        struct const_iobuf *req,
        struct iobuf *res);

static HRESULT felica_cmd_read_without_encryption(
        struct felica *f,
        struct const_iobuf *req,
        struct iobuf *res);

static HRESULT felica_cmd_write_without_encryption(
    struct felica* f,
    struct const_iobuf* req,
    struct iobuf* res);

uint64_t felica_get_amusement_ic_PMm(bool is_mobile)
{
    /*
     * AIC Card PMm, if this is returned from the card,
     * the aimelib will access the actual blocks for authentication.
     */

    return is_mobile ? 0x0018000000014300 : 0x00F1000000014300;
}

HRESULT felica_transact(
        struct felica *f,
        struct const_iobuf *req,
        struct iobuf *res)
{
    uint64_t IDm;
    uint8_t code;
    HRESULT hr;

    assert(f != NULL);
    assert(req != NULL);
    assert(res != NULL);

    hr = iobuf_read_8(req, &code);

    if (FAILED(hr)) {
        return hr;
    }

    hr = iobuf_write_8(res, code + 1);

    if (FAILED(hr)) {
        return hr;
    }

    if (code != FELICA_CMD_POLL) {
        hr = iobuf_read_be64(req, &IDm);

        if (FAILED(hr)) {
            return hr;
        }

        if (IDm != f->IDm) {
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }

        hr = iobuf_write_be64(res, IDm);

        if (FAILED(hr)) {
            return hr;
        }
    }

    switch (code) {
    case FELICA_CMD_POLL:
        return felica_cmd_poll(f, req, res);

    case FELICA_CMD_GET_SYSTEM_CODE:
        return felica_cmd_get_system_code(f, req, res);

    case FELICA_READ_WITHOUT_ENCRYPTION:
        return felica_cmd_read_without_encryption(f, req, res);

    case FELICA_WRITE_WITHOUT_ENCRYPTION:
        return felica_cmd_write_without_encryption(f, req, res);

    case FELICA_CMD_ACTIVE:
        return felica_cmd_active(f, req, res);

    default:
        dprintf("FeliCa: Unimplemented command %02x, payload:\n", code);
        dump_const_iobuf(req);

        return E_NOTIMPL;
    }
}

static HRESULT felica_cmd_poll(
        struct felica *f,
        struct const_iobuf *req,
        struct iobuf *res)
{
    uint16_t system_code;
    uint8_t request_code;
    uint8_t time_slot;
    HRESULT hr;

    /* Request: */

    hr = iobuf_read_be16(req, &system_code);

    if (FAILED(hr)) {
        return hr;
    }

    hr = iobuf_read_8(req, &request_code);

    if (FAILED(hr)) {
        return hr;
    }

    hr = iobuf_read_8(req, &time_slot);

    if (FAILED(hr)) {
        return hr;
    }

    if (system_code != 0xFFFF && system_code != f->system_code) {
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    // TODO handle other params correctly...

    /* Response: */

    hr = iobuf_write_be64(res, f->IDm);

    if (FAILED(hr)) {
        return hr;
    }

    hr = iobuf_write_be64(res, f->PMm);

    if (FAILED(hr)) {
        return hr;
    }

    if (request_code == 0x01) {
        hr = iobuf_write_be16(res, f->system_code);

        if (FAILED(hr)) {
            return hr;
        }
    }

    return S_OK;
}

static HRESULT felica_cmd_get_system_code(
        struct felica *f,
        struct const_iobuf *req,
        struct iobuf *res)
{
    HRESULT hr;

    hr = iobuf_write_8(res, 1); /* Number of system codes */

    if (FAILED(hr)) {
        return hr;
    }

    hr = iobuf_write_be16(res, f->system_code);

    if (FAILED(hr)) {
        return hr;
    }

    return S_OK;
}

static HRESULT felica_cmd_read_without_encryption(
        struct felica *f,
        struct const_iobuf *req,
        struct iobuf *res)
{
    HRESULT hr;
    uint8_t system_code_count;
    uint16_t* system_codes;
    uint8_t read_block_count;
    uint8_t* blocks;
    size_t i;

    hr = iobuf_read_8(req, &system_code_count);

    if (FAILED(hr)) {
        goto fail;
    }

    system_codes = malloc(sizeof(uint16_t) * system_code_count);
    if (!system_codes) goto fail;

    for (i = 0; i < system_code_count; i++) {
        hr = iobuf_read_be16(req, system_codes + i);

        if (FAILED(hr)) {
            goto fail;
        }
    }

    hr = iobuf_read_8(req, &read_block_count);

    if (FAILED(hr)) {
        goto fail;
    }

    blocks = malloc(read_block_count);
    if (!system_codes) goto fail;

    for (i = 0; i < read_block_count; i++) {
        // 0x80
        hr = iobuf_read_8(req, blocks + i);

        if (FAILED(hr)) {
            goto fail;
        }

        // actual block num
        hr = iobuf_read_8(req, blocks + i);

        if (FAILED(hr)) {
            goto fail;
        }
    }

    // status
    hr = iobuf_write_be16(res, 0);

    if (FAILED(hr)) {
        goto fail;
    }

    // block count
    hr = iobuf_write_8(res, read_block_count);

    if (FAILED(hr)) {
        goto fail;
    }

    // block data
    for (i = 0; i < read_block_count; i++)
    {
        dprintf("FeliCa: Read block %x\n", blocks[i]);

        switch (blocks[i]) {
            case 0x82: {
                hr = iobuf_write_be64(res, f->IDm);

                if (FAILED(hr))
                {
                    goto fail;
                }

                hr = iobuf_write_be64(res, 0x0078000000000000ull);

                if (FAILED(hr))
                {
                    goto fail;
                }
            }
            default: {
                hr = iobuf_write_be64(res, 0);

                if (FAILED(hr))
                {
                    goto fail;
                }

                hr = iobuf_write_be64(res, 0);

                if (FAILED(hr))
                {
                    goto fail;
                }
            }
        }
    }

    hr = S_OK;

fail:
    if (system_codes) free(system_codes);
    if (blocks) free(blocks);

    return hr;
}

static HRESULT felica_cmd_write_without_encryption(
    struct felica* f,
    struct const_iobuf* req,
    struct iobuf* res)
{
    return iobuf_write_be16(res, 0);
}

static HRESULT felica_cmd_active(
        struct felica *f,
        struct const_iobuf *req,
        struct iobuf *res)
{
    /* The specification for this command is probably only available under NDA.
       Returning what the driver seems to want. */

    return iobuf_write_8(res, 0);
}
