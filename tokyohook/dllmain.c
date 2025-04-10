/*
    "Mario & Sonic at the Tokyo 2020 Olympics Arcade" (tokyo) hook

    Devices

     USB:    837-15257 "Type 4" I/O Board
    COM1:    837-15093-04 LED Controller Board
*/

#include <windows.h>

#include <stdlib.h>

#include "board/io4.h"

#include "hook/process.h"

#include "hooklib/dvd.h"
#include "hooklib/serial.h"
#include "hooklib/spike.h"

#include "tokyohook/config.h"
#include "tokyohook/io4.h"
#include "tokyohook/tokyo-dll.h"

#include "platform/platform.h"

#include "util/dprintf.h"
#include "util/env.h"

static HMODULE tokyo_hook_mod;
static process_entry_t tokyo_startup;
static struct tokyo_hook_config tokyo_hook_cfg;

static DWORD CALLBACK tokyo_pre_startup(void)
{
    HRESULT hr;

    dprintf("--- Begin tokyo_pre_startup ---\n");

    /* Load config */

    tokyo_hook_config_load(&tokyo_hook_cfg, get_config_path());

    /* Hook Win32 APIs */

    dvd_hook_init(&tokyo_hook_cfg.dvd, tokyo_hook_mod);
    zinput_hook_init(&tokyo_hook_cfg.zinput);
    serial_hook_init();

    /* Initialize emulation hooks */

    struct dipsw_config new_dipsw_config[8] = {
        {L"Delivery Server", L"Server", L"Client"},
        {L"Cabinet ID Setting", L"ON", L"OFF"},
        {L"Cabinet ID Setting", L"ON", L"OFF"},
    };

    // Set the system dip switch configuration
    memcpy(tokyo_hook_cfg.platform.system.dipsw_config, new_dipsw_config,
           sizeof(new_dipsw_config));

    hr = platform_hook_init(
            &tokyo_hook_cfg.platform,
            "SDFV",
            "ACA1",
            tokyo_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    bool *dipsw = &tokyo_hook_cfg.platform.system.dipsw[0];
    unsigned int cabinet_id = 0;

    if (dipsw[0] == 1 && dipsw[1] == 0 && dipsw[2] == 0) {
        cabinet_id = 1; // Server 1
    } else if (dipsw[0] == 0 && dipsw[1] == 1 && dipsw[2] == 0) {
        cabinet_id = 2; // Client 2
    } else if (dipsw[0] == 0 && dipsw[1] == 0 && dipsw[2] == 1) {
        cabinet_id = 3; // Client 3
    } else if (dipsw[0] == 0 && dipsw[1] == 1 && dipsw[2] == 1) {
        cabinet_id = 4; // Client 4
    } else {
        dprintf("Error: Invalid dip switch configuration!\n");
    }

    // Print the correct Cabinet ID if valid
    if (cabinet_id > 0) {
        dprintf("System: Cabinet ID %d\n", cabinet_id);
    }

    hr = tokyo_dll_init(&tokyo_hook_cfg.dll, tokyo_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    unsigned int led_port_no[2] = {1, 0};
    hr = led15093_hook_init(&tokyo_hook_cfg.led15093, 
        tokyo_dll.led_init, tokyo_dll.led_set_leds, led_port_no);

    if (FAILED(hr)) {
        return hr;
    }

    hr = tokyo_io4_hook_init(&tokyo_hook_cfg.io4);

    if (FAILED(hr)) {
        goto fail;
    }

    /* Initialize debug helpers */

    spike_hook_init(get_config_path());

    dprintf("---  End  tokyo_pre_startup ---\n");

    /* Jump to EXE start address */

    return tokyo_startup();

fail:
    ExitProcess(EXIT_FAILURE);
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx)
{
    HRESULT hr;

    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    tokyo_hook_mod = mod;

    hr = process_hijack_startup(tokyo_pre_startup, &tokyo_startup);

    if (!SUCCEEDED(hr)) {
        dprintf("Failed to hijack process startup: %x\n", (int) hr);
    }

    return SUCCEEDED(hr);
}
