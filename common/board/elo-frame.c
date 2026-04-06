/*
    SmartSet protocol framing implementation
    
    The SmartSet protocol uses the following frame format:
    [0x55] [CMD] [DATA...] [CHECKSUM]
    
    - Lead byte: 0x55 (alternating bit pattern 01010101)
    - Command byte: ASCII command character
    - Data: Variable length based on command
    - Checksum: Sum of all bytes (including lead) + 0xAA, low byte only
*/

#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board/elo-cmd.h"
#include "board/elo-frame.h"

#include "hook/iobuf.h"

#include "util/dprintf.h"

static void elo_frame_sync(struct iobuf *src);
static HRESULT elo_frame_accept(const struct iobuf *dest);

/* Checksum offset to cancel out the lead byte */
#define ELO_CHECKSUM_OFFSET 0xAA

/* SmartSet frame length */
#define ELO_FRAME_LENGTH 10

static void elo_frame_sync(struct iobuf *src)
{
    size_t i;

    for (i = 0; i < src->pos && src->bytes[i] != ELO_FRAME_LEAD; i++);

    src->pos -= i;
    memmove(&src->bytes[0], &src->bytes[i], i);
}

static HRESULT elo_frame_accept(const struct iobuf *dest)
{
    uint8_t checksum;
    uint8_t calc_checksum;
    size_t i;

    if (dest->pos < ELO_FRAME_LENGTH) {
        return S_FALSE;
    }

    /* Try to validate checksum with current buffer length
       Calculate checksum (sum of all bytes including lead + 0xAA) */
    calc_checksum = ELO_CHECKSUM_OFFSET;

    for (i = 0; i < dest->pos - 1; i++) {
        calc_checksum += dest->bytes[i];
    }

    calc_checksum &= 0xFF;

    /* Check if last byte matches calculated checksum */
    checksum = dest->bytes[dest->pos - 1];

    if (checksum != calc_checksum) {
        dprintf("Checksum missmatch: %d != %d\n", checksum, calc_checksum);
        return S_FALSE;
    }

    return S_OK;
}

HRESULT elo_frame_decode(struct iobuf *dest, struct iobuf *src)
{
    uint8_t byte;
    size_t i;
    HRESULT hr;

    assert(dest != NULL);
    assert(dest->bytes != NULL || dest->nbytes == 0);
    assert(dest->pos <= dest->nbytes);
    assert(src != NULL);
    assert(src->bytes != NULL || src->nbytes == 0);
    assert(src->pos <= src->nbytes);

    elo_frame_sync(src);

    dest->pos = 0;

    for (i = 0, hr = S_FALSE; i < src->pos && hr == S_FALSE; i++) {
        byte = src->bytes[i];

        if (dest->pos >= dest->nbytes) {
            hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        } else if (i == 0 && byte != ELO_FRAME_LEAD) {
            /* Invalid lead byte */
            hr = E_FAIL;
        } else {
            dest->bytes[dest->pos++] = byte;
        }

        if (SUCCEEDED(hr)) {
            hr = elo_frame_accept(dest);
        }
    }

    if (hr != S_FALSE) {
        memmove(&src->bytes[0], &src->bytes[i], src->pos - i);
        src->pos -= i;

        /* If accepted, remove checksum from destination */
        if (hr == S_OK) {
            dest->pos--;
        }
    }

    return hr;
}

HRESULT elo_frame_encode(
        struct iobuf *dest,
        const void *ptr,
        size_t nbytes)
{
    const uint8_t *src;
    uint8_t checksum;
    size_t i;

    assert(dest != NULL);
    assert(dest->bytes != NULL || dest->nbytes == 0);
    assert(dest->pos <= dest->nbytes);
    assert(ptr != NULL);

    src = ptr;

    /* Requires exactly 10 bytes */
    if (dest->pos + ELO_FRAME_LENGTH > dest->nbytes) {
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }

    /* First byte must be lead-in */
    assert(nbytes >= 1 && src[0] == ELO_FRAME_LEAD);

    checksum = ELO_CHECKSUM_OFFSET;

    /* Write exactly 9 bytes (lead-in + 8 data bytes) */
    for (i = 0; i < ELO_FRAME_LENGTH-1; i++) {
        uint8_t b = (i < nbytes) ? src[i] : 0x00;
        dest->bytes[dest->pos++] = b;
        checksum += b;
    }

    /* Append checksum as 10th byte */
    dest->bytes[dest->pos++] = checksum;

    return S_OK;
}
