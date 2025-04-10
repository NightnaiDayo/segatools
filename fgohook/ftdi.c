/*
    SEGA 837-14509-02 USB -> Serial Adapter hook

    The 837-15093-06 LED controller is connected to the ALLS with an adapter.
    This tiny board has a FTDI FT232BL chip, and is referenced in schematics as
    "USB-SER I/F BD".

    The game queries the presence of the FTDI board itself, followed by a
    registry check to see which port number is assigned to the FTDI board.
    If these fail, the "CABINET LED" check on startup will always return "NG".

    Credits:

    OLEG
*/

#include <windows.h>
#include <setupapi.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "fgohook/ftdi.h"

#include "hook/iohook.h"
#include "hook/table.h"

#include "hooklib/setupapi.h"

#include "util/dprintf.h"

static struct ftdi_config ftdi_cfg;


static BOOL WINAPI hook_SetupDiGetDeviceRegistryPropertyA(
        HDEVINFO DeviceInfoSet,
        PSP_DEVINFO_DATA DeviceInfoData,
        uint32_t Property,
        uint32_t *PropertyRegDataType,
        void *PropertyBuffer,
        uint32_t PropertyBufferSize,
        uint32_t *RequiredSize
);

static HKEY WINAPI hook_SetupDiOpenDevRegKey(
        HDEVINFO DeviceInfoSet,
        PSP_DEVINFO_DATA DeviceInfoData,
        uint32_t Scope,
        uint32_t HwProfile,
        uint32_t KeyType,
        REGSAM samDesired
);

static LSTATUS WINAPI hook_RegQueryValueExA(
        HKEY handle,
        const char *name,
        void *reserved,
        uint32_t *type,
        void *bytes,
        uint32_t *nbytes);

static LSTATUS WINAPI hook_RegCloseKey(HKEY handle);


static BOOL (WINAPI *next_SetupDiGetDeviceRegistryPropertyA)(
        HDEVINFO DeviceInfoSet,
        PSP_DEVINFO_DATA DeviceInfoData,
        uint32_t Property,
        uint32_t *PropertyRegDataType,
        void *PropertyBuffer,
        uint32_t PropertyBufferSize,
        uint32_t *RequiredSize
);

static HKEY (WINAPI *next_SetupDiOpenDevRegKey)(
        HDEVINFO DeviceInfoSet,
        PSP_DEVINFO_DATA DeviceInfoData,
        uint32_t Scope,
        uint32_t HwProfile,
        uint32_t KeyType,
        REGSAM samDesired
);

static LSTATUS (WINAPI *next_RegQueryValueExA)(
        HKEY handle,
        const char *name,
        void *reserved,
        uint32_t *type,
        void *bytes,
        uint32_t *nbytes);

static LSTATUS (WINAPI *next_RegCloseKey)(HKEY handle);


static const struct hook_symbol setupapi_syms[] = {
        {
                .name = "SetupDiGetDeviceRegistryPropertyA",
                .patch = hook_SetupDiGetDeviceRegistryPropertyA,
                .link = (void *) &next_SetupDiGetDeviceRegistryPropertyA,
        }, {
                .name = "SetupDiOpenDevRegKey",
                .patch = hook_SetupDiOpenDevRegKey,
                .link = (void *) &next_SetupDiOpenDevRegKey,
        }
};

static const struct hook_symbol reg_syms[] = {
        {
                .name = "RegQueryValueExA",
                .patch = hook_RegQueryValueExA,
                .link = (void *) &next_RegQueryValueExA,
        }, {
                .name = "RegCloseKey",
                .patch = hook_RegCloseKey,
                .link = (void *) &next_RegCloseKey,
        }
};

const size_t device_fake_key = 0xDEADBEEF;

static HANDLE ftdi_fd;
static char port_name[8];

HRESULT ftdi_hook_init(const struct ftdi_config *cfg, unsigned int port_no) {
    HRESULT hr;

    assert(cfg != NULL);

    if (!cfg->enable) {
        return S_FALSE;
    }

    if (cfg->port_no != 0) {
        port_no = cfg->port_no;
    }

    hook_table_apply(
            NULL,
            "setupapi.dll",
            setupapi_syms,
            _countof(setupapi_syms));

    hook_table_apply(
            NULL,
            "advapi32.dll",
            reg_syms,
            _countof(reg_syms));

    memcpy(&ftdi_cfg, cfg, sizeof(*cfg));

    hr = iohook_open_nul_fd(&ftdi_fd);

    if (FAILED(hr)) {
        return hr;
    }

    hr = setupapi_add_phantom_dev(&ftdi_guid, L"$ftdi");

    if (FAILED(hr)) {
        return hr;
    }

    sprintf(port_name, "COM%d", port_no);

    dprintf("FTDI: Hook enabled.\n");
    return S_OK;
}

static BOOL WINAPI hook_SetupDiGetDeviceRegistryPropertyA(
        HDEVINFO DeviceInfoSet,
        PSP_DEVINFO_DATA DeviceInfoData,
        uint32_t Property,
        uint32_t *PropertyRegDataType,
        void *PropertyBuffer,
        uint32_t PropertyBufferSize,
        uint32_t *RequiredSize
) {
    if (!PropertyBuffer || PropertyBufferSize == 0) {
        *RequiredSize = 16;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    *PropertyRegDataType = 1;
    return TRUE;
}

static HKEY WINAPI hook_SetupDiOpenDevRegKey(
        HDEVINFO DeviceInfoSet,
        PSP_DEVINFO_DATA DeviceInfoData,
        uint32_t Scope,
        uint32_t HwProfile,
        uint32_t KeyType,
        REGSAM samDesired
) {
    return (HKEY) device_fake_key;
}

static LSTATUS WINAPI hook_RegCloseKey(HKEY handle) {
    if (handle == (HKEY) device_fake_key)
        return ERROR_SUCCESS;

    return next_RegCloseKey(handle);
}

static LSTATUS WINAPI hook_RegQueryValueExA(
        HKEY handle,
        const char *name,
        void *reserved,
        uint32_t *type,
        void *bytes,
        uint32_t *nbytes) {
    if (handle == (HKEY) device_fake_key && !strcmp(name, "PortName")) {
        strcpy(bytes, port_name);
        *nbytes = 5;
        *type = 1;
        return ERROR_SUCCESS;
    }

    return next_RegQueryValueExA(
            handle,
            name,
            reserved,
            type,
            bytes,
            nbytes);
}
