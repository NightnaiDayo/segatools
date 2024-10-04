/*
    "SEGA World Drivers Championship" (swdc) hook

    Devices

    USB:    837-15257 "Type 4" I/O Board
    USB:    838-15415 Indicator BD Main Board (COM21)
            WITH
            838-15416 Indicator BD LED Board
    COM1:   838-15069 MOTOR DRIVE BD RS232/422 board
    COM2:   837-15070-04 IC BD LED controller board
    COM3:   837-15396 "Gen 3" Aime reader
    COM4:   200-6275 VFD GP1232A02A FUTABA board
*/

#include <windows.h>
#include <shlwapi.h>

#include <stdlib.h>

#include "board/sg-reader.h"
#include "board/io4.h"
#include "board/vfd.h"

#include "hook/process.h"

#include "hooklib/dvd.h"
#include "hooklib/serial.h"
#include "hooklib/spike.h"

#include "swdchook/config.h"
#include "swdchook/swdc-dll.h"
#include "swdchook/io4.h"
#include "swdchook/ffb.h"

#include "platform/platform.h"

#include "util/dprintf.h"

static HMODULE swdc_hook_mod;
static process_entry_t swdc_startup;
static struct swdc_hook_config swdc_hook_cfg;

static DWORD CALLBACK swdc_pre_startup(void)
{
    HRESULT hr;

    dprintf("--- Begin swdc_pre_startup ---\n");

    /* Config load */

    swdc_hook_config_load(&swdc_hook_cfg, L".\\segatools.ini");

    /* Hook Win32 APIs */

    serial_hook_init();
    dvd_hook_init(&swdc_hook_cfg.dvd, swdc_hook_mod);

    /* Initialize emulation hooks */

    hr = platform_hook_init(
            &swdc_hook_cfg.platform,
            "SDDS",
            "ACA4",
            swdc_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = swdc_dll_init(&swdc_hook_cfg.dll, swdc_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = sg_reader_hook_init(&swdc_hook_cfg.aime, 3, 3, swdc_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = vfd_hook_init(&swdc_hook_cfg.vfd, 4);

    if (FAILED(hr)) {
        return hr;
    }

    hr = swdc_io4_hook_init(&swdc_hook_cfg.io4);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = swdc_ffb_hook_init(&swdc_hook_cfg.ffb, 1);

    if (FAILED(hr)) {
        goto fail;
    }

    /* Not working, different board -04 instead of -02? */
    hr = led15070_hook_init(&swdc_hook_cfg.led15070, swdc_dll.led_init, 
        swdc_dll.led_set_fet_output, NULL, swdc_dll.led_gs_update, 2, 1);

    if (FAILED(hr)) {
        goto fail;
    }

    /* Hook external DLL APIs */

    zinput_hook_init(&swdc_hook_cfg.zinput);

    /* Initialize debug helpers */

    spike_hook_init(L".\\segatools.ini");

    dprintf("---  End  swdc_pre_startup ---\n");

    /* Jump to EXE start address */

    return swdc_startup();

fail:
    ExitProcess(EXIT_FAILURE);
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx)
{
    HRESULT hr;

    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    swdc_hook_mod = mod;

    hr = process_hijack_startup(swdc_pre_startup, &swdc_startup);

    if (!SUCCEEDED(hr)) {
        dprintf("Failed to hijack process startup: %x\n", (int) hr);
    }

    return SUCCEEDED(hr);
}
