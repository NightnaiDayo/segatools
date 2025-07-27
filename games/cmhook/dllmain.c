/*
    "Card Maker" (cm) hook

    Devices

    USB:    837-15257-01 "Type 4" I/O Board
    USB:    838-20006 "WinTouch" Controller Board
    USB:    630-00009 Sinfonia CHC-C310 Printer
    COM1:   837-15396 "Gen 3" Aime Reader
    COM2:   200-6275 VFD GP1232A02A FUTABA Board
    COM3:   220-5872 AS-6DB Coin Selector
*/

#include <windows.h>

#include <stdlib.h>

#include "board/io4.h"
#include "board/sg-reader.h"
#include "board/vfd.h"

#include "hook/process.h"

#include "hooklib/dvd.h"
#include "hooklib/touch.h"
#include "hooklib/serial.h"
#include "hooklib/spike.h"

#include "cmhook/config.h"
#include "cmhook/io4.h"
#include "cmhook/cm-dll.h"

#include "platform/platform.h"

#include "unityhook/hook.h"

#include "util/dprintf.h"
#include "util/env.h"

static HMODULE cm_hook_mod;
static process_entry_t cm_startup;
static struct cm_hook_config cm_hook_cfg;

static DWORD CALLBACK cm_pre_startup(void)
{
    HRESULT hr;

    dprintf("--- Begin cm_pre_startup ---\n");

    /* Load config */

    cm_hook_config_load(&cm_hook_cfg, get_config_path());

    /* Hook Win32 APIs */

    dvd_hook_init(&cm_hook_cfg.dvd, cm_hook_mod);
    touch_screen_hook_init(&cm_hook_cfg.touch, cm_hook_mod);
    serial_hook_init();

    /* Hook external DLL APIs */

    printer_hook_init(&cm_hook_cfg.printer, 0, cm_hook_mod);

    /* Initialize emulation hooks */

    struct dipsw_config new_dipsw_config[8] = {
        {L"Delivery Server", L"Client", L"Server"},
    };

    // Set the system dip switch configuration
    memcpy(cm_hook_cfg.platform.system.dipsw_config, new_dipsw_config,
           sizeof(new_dipsw_config));

    hr = platform_hook_init(
            &cm_hook_cfg.platform,
            "SDED",
            "ACA1",
            cm_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = cm_dll_init(&cm_hook_cfg.dll, cm_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = sg_reader_hook_init(&cm_hook_cfg.aime, 1, 3, cm_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = vfd_hook_init(&cm_hook_cfg.vfd, 2);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = cm_io4_hook_init(&cm_hook_cfg.io4);

    if (FAILED(hr)) {
        goto fail;
    }

    /* Initialize Unity native plugin DLL hooks

       There seems to be an issue with other DLL hooks if `LoadLibraryW` is
       hooked earlier in the `cmhook` initialization. */

    unity_hook_init(&cm_hook_cfg.unity, cm_hook_mod, NULL);

    /* Initialize debug helpers */

    spike_hook_init(get_config_path());

    dprintf("---  End  cm_pre_startup ---\n");

    /* Jump to EXE start address */

    return cm_startup();

fail:
    ExitProcess(EXIT_FAILURE);
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx)
{
    HRESULT hr;

    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    cm_hook_mod = mod;

    hr = process_hijack_startup(cm_pre_startup, &cm_startup);

    if (!SUCCEEDED(hr)) {
        dprintf("Failed to hijack process startup: %x\n", (int) hr);
    }

    return SUCCEEDED(hr);
}
