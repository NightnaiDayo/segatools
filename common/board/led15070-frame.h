#pragma once

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include "hook/iobuf.h"

enum {
    LED_15070_FRAME_SYNC = 0xE0,
};

struct led15070_hdr {
    uint8_t sync;
    uint8_t dest_adr;
    uint8_t src_adr;
    uint8_t nbytes;
};

HRESULT led15070_frame_decode(struct iobuf *dest, struct iobuf *src);

HRESULT led15070_frame_encode(
        struct iobuf *dest,
        const void *ptr,
        size_t nbytes);
