#pragma once


#include <windows.h>
#include <stdint.h>

#include "hook/iobuf.h"

/* SmartSet Protocol Lead-in byte */
enum {
    ELO_FRAME_LEAD = 0x55
};

struct elo_packet_hdr {
    uint8_t lead;
    uint8_t cmd;
};

HRESULT elo_frame_decode(struct iobuf *dest, struct iobuf *src);

HRESULT elo_frame_encode(struct iobuf *dest, const void *src, size_t nbytes);
