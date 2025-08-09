/*
    "Initial D THE ARCADE" (idac) hook

    Devices

     USB:   837-15257 "Type 4" I/O Board
    COM1:   838-15069 MOTOR DRIVE BD RS232/422 Board
    COM2:   837-15070-02 IC BD LED Controller Board (DIPSW2 OFF)
            OR
            837-15070-04 IC BD LED Controller Board (DIPSW2 ON)
    COM3:   837-15286 "Gen 2" Aime Reader (DIPSW2 OFF)
            OR
            837-15396 "Gen 3" Aime Reader (DIPSW2 ON)
*/

#include <windows.h>
#include <shlwapi.h>

#include <stdlib.h>

#include "board/sg-reader.h"
#include "board/io4.h"

#include "hook/process.h"

#include "hooklib/dvd.h"
#include "hooklib/serial.h"
#include "hooklib/spike.h"

#include "idachook/config.h"
#include "idachook/idac-dll.h"
#include "idachook/io4.h"
#include "idachook/ffb.h"

#include "platform/platform.h"

#include "util/dprintf.h"
#include "util/env.h"

static HMODULE idac_hook_mod;
static process_entry_t idac_startup;
static struct idac_hook_config idac_hook_cfg;

static DWORD CALLBACK idac_pre_startup(void)
{
    HRESULT hr;

    dprintf("--- Begin idac_pre_startup ---\n");

    /* Config load */

    idac_hook_config_load(&idac_hook_cfg, get_config_path());

    /* Hook Win32 APIs */

    serial_hook_init();
    dvd_hook_init(&idac_hook_cfg.dvd, idac_hook_mod);

    /* Initialize emulation hooks */

    struct dipsw_config new_dipsw_config[8] = {
        {L"Delivery Server", L"Server", L"Client"},
        {L"Cabinet Type", L"SWDC CVT", L"DZero CVT"},
        {L"Single Seat", L"ON", L"OFF"},
        {L"Seat Setting 1", L"ON", L"OFF"},
        {L"Seat Setting 2", L"ON", L"OFF"},
    };

    // Set the system dip switch configuration
    memcpy(idac_hook_cfg.platform.system.dipsw_config, new_dipsw_config,
           sizeof(new_dipsw_config));

    hr = platform_hook_init(
            &idac_hook_cfg.platform,
            "SDGT",
            "ACA4",
            idac_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    bool *dipsw = &idac_hook_cfg.platform.system.dipsw[0];
    bool is_swdc_cvt = dipsw[1];

    if (!dipsw[2]) {
        // the next two bit are the seat number most significant bit first
        dprintf("System: Seat Number: %d\n", ((dipsw[4] << 1) | dipsw[3]) + 1);
    }

    hr = idac_dll_init(&idac_hook_cfg.dll, idac_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = sg_reader_hook_init(&idac_hook_cfg.aime, 3, is_swdc_cvt ? 3 : 2, idac_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = idac_io4_hook_init(&idac_hook_cfg.io4);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = idac_ffb_hook_init(&idac_hook_cfg.ffb, 1);

    if (FAILED(hr)) {
        goto fail;
    }

    unsigned int led_port_no[2] = {2, 0};
    hr = led15070_hook_init(&idac_hook_cfg.led15070, idac_dll.led_init, 
        idac_dll.led_set_fet_output, NULL, idac_dll.led_gs_update, led_port_no);

    if (FAILED(hr)) {
        goto fail;
    }

    /* Initialize native plugin DLL hooks

       There seems to be an issue with other DLL hooks if `LoadLibraryW` is
       hooked earlier in the initialization. */

    indrun_hook_init(&idac_hook_cfg.indrun);

    /* Initialize debug helpers */

    spike_hook_init(get_config_path());

    dprintf("---  End  idac_pre_startup ---\n");

    /* Jump to EXE start address */

    return idac_startup();

fail:
    ExitProcess(EXIT_FAILURE);
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx)
{
    HRESULT hr;

    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    idac_hook_mod = mod;

    hr = process_hijack_startup(idac_pre_startup, &idac_startup);

    if (!SUCCEEDED(hr)) {
        dprintf("Failed to hijack process startup: %x\n", (int) hr);
    }

    return SUCCEEDED(hr);
}
