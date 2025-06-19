#pragma once

#include <windows.h>
#include <initguid.h>

#include <stdbool.h>
#include <stddef.h>

struct ftdi_config {
    bool enable;
    uint32_t port_no;
};

DEFINE_GUID(
        ftdi_guid,
        0x86E0D1E0,
        0x8089,
        0x11D0,
        0x9C, 0xE4, 0x08, 0x00, 0x3E, 0x30, 0x1F, 0x73);

HRESULT ftdi_hook_init(const struct ftdi_config *cfg, unsigned int port_no);
