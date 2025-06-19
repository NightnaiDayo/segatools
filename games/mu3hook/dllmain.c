/*
    "O.N.G.E.K.I." (mu3) hook

    Devices

    USB:    837-15257-01 "Type 4" I/O Board
    USB:    3 * 601-13216 USB "QR Code" Camera (SDDT1-SDDT3)
    COM1:   837-15396 "Gen 3" Aime Reader
    COM2:   200-6275 VFD GP1232A02A FUTABA Board
    COM3:   837-15093-06 LED Controller Board
*/

#include <windows.h>

#include <stdlib.h>

#include "board/sg-reader.h"
#include "board/vfd.h"

#include "gfxhook/d3d9.h"
#include "gfxhook/d3d11.h"
#include "gfxhook/dxgi.h"
#include "gfxhook/gfx.h"

#include "hook/process.h"

#include "hooklib/dvd.h"
#include "hooklib/serial.h"
#include "hooklib/spike.h"

#include "mu3hook/config.h"
#include "mu3hook/io4.h"
#include "mu3hook/mu3-dll.h"

#include "platform/platform.h"

#include "unityhook/config.h"
#include "unityhook/hook.h"

#include "util/dprintf.h"
#include "util/env.h"

static HMODULE mu3_hook_mod;
static process_entry_t mu3_startup;
static struct mu3_hook_config mu3_hook_cfg;

static DWORD CALLBACK mu3_pre_startup(void)
{
    HRESULT hr;

    dprintf("--- Begin mu3_pre_startup ---\n");

    /* Load config */

    mu3_hook_config_load(&mu3_hook_cfg, get_config_path());

    /* Hook Win32 APIs */

    dvd_hook_init(&mu3_hook_cfg.dvd, mu3_hook_mod);
    gfx_hook_init(&mu3_hook_cfg.gfx);
    gfx_d3d9_hook_init(&mu3_hook_cfg.gfx, mu3_hook_mod);
    gfx_d3d11_hook_init(&mu3_hook_cfg.gfx, mu3_hook_mod);
    gfx_dxgi_hook_init(&mu3_hook_cfg.gfx, mu3_hook_mod);
    serial_hook_init();

    /* Initialize emulation hooks */

    struct dipsw_config new_dipsw_config[8] = {
        {L"Delivery Server", L"Server", L"Client"},
    };

    // Set the system dip switch configuration
    memcpy(mu3_hook_cfg.platform.system.dipsw_config, new_dipsw_config,
           sizeof(new_dipsw_config));

    hr = platform_hook_init(
            &mu3_hook_cfg.platform,
            "SDDT",
            "ACA1",
            mu3_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = mu3_dll_init(&mu3_hook_cfg.dll, mu3_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    unsigned int led_port_no[2] = {3, 0};
    hr = led15093_hook_init(&mu3_hook_cfg.led15093, 
        mu3_dll.led_init, mu3_dll.led_set_leds, led_port_no);

    if (FAILED(hr)) {
        return hr;
    }

    hr = sg_reader_hook_init(&mu3_hook_cfg.aime, 1, 1, mu3_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = vfd_hook_init(&mu3_hook_cfg.vfd, 2);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = mu3_io4_hook_init(&mu3_hook_cfg.io4);

    if (FAILED(hr)) {
        goto fail;
    }

    /* Initialize Unity native plugin DLL hooks

       There seems to be an issue with other DLL hooks if `LoadLibraryW` is
       hooked earlier in the `mu3hook` initialization. */

    unity_hook_init(&mu3_hook_cfg.unity, mu3_hook_mod, NULL);

    /* Initialize debug helpers */

    spike_hook_init(get_config_path());

    dprintf("---  End  mu3_pre_startup ---\n");

    /* Jump to EXE start address */

    return mu3_startup();

fail:
    ExitProcess(EXIT_FAILURE);
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx)
{
    HRESULT hr;

    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    mu3_hook_mod = mod;

    hr = process_hijack_startup(mu3_pre_startup, &mu3_startup);

    if (!SUCCEEDED(hr)) {
        dprintf("Failed to hijack process startup: %x\n", (int) hr);
    }

    return SUCCEEDED(hr);
}
