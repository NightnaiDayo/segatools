#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "iccard/aime.h"
#include "iccard/mifare.h"
#include "iccard/solitaire.h"

#include "util/dprintf.h"

HRESULT aime_card_populate(
        struct mifare *mifare,
        const uint8_t *luid,
        size_t nbytes)
{
    uint8_t b;
    size_t i;
    char accessCode[21];
    char hashed_id_wrk[9];
    char id_wrk[9];

    assert(mifare != NULL);
    assert(luid != NULL);

    memset(mifare, 0, sizeof(*mifare));

    if (nbytes != 10) {
        dprintf("AiMe IC: LUID must be 10 bytes\n");

        return E_INVALIDARG;
    }

    for (i = 0 ; i < 10 ; i++) {
        b = luid[i];

        if ((b & 0xF0) > 0x90 || (b & 0x0F) > 0x09) {
            dprintf("AiMe IC: LUID must be binary-coded decimal\n");
            return E_INVALIDARG;
        }

        mifare->sectors[0].blocks[2].bytes[6 + i] = b;
    }
    
    // Set the card ID, nothing else matters in the first block
    mifare->sectors[0].blocks[0].bytes[0] = luid[0];
    mifare->sectors[0].blocks[0].bytes[1] = luid[1];
    mifare->sectors[0].blocks[0].bytes[2] = luid[2];
    mifare->sectors[0].blocks[0].bytes[3] = luid[3];

    sprintf_s(accessCode, sizeof accessCode, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
        luid[0], luid[1], luid[2], luid[3], luid[4], luid[5], luid[6], luid[7], luid[8], luid[9]);

        memcpy_s(hashed_id_wrk, sizeof(hashed_id_wrk), &accessCode[5],
            8);
    
    hashed_id_wrk[8] = '\0';

    SolitaireCipherDecode(&accessCode[13], hashed_id_wrk, id_wrk);


    DWORD nSerial = atoi(id_wrk);

    mifare->sectors[0].blocks[1].bytes[12] = (nSerial >> 24) & 0xff;
    mifare->sectors[0].blocks[1].bytes[13] = (nSerial >> 16) & 0xff;
    mifare->sectors[0].blocks[1].bytes[14] = (nSerial >> 8) & 0xff;
    mifare->sectors[0].blocks[1].bytes[15] = nSerial & 0xff;

    return S_OK;
}
