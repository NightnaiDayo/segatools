/*
    "Soul Reverse" (soul) hook

    Devices

    USB:    837-15257-01 "Type 4" I/O Board

    [Satellite]

    COM1:   200-6275 VFD GP1232A02A FUTABA Board
    COM2:   837-15093-06 LED Controller Board
    COM3:   837-15396 "Gen 3" Aime Reader

    [Terminal]

    COM1:   837-15396 "Gen 3" Aime Reader
    COM3:   837-15093-06 LED Controller Board
    COM4:   200-6275 VFD GP1232A02A FUTABA Board

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

#include "gfxhook/gfx.h"

#include "soulhook/config.h"
#include "soulhook/io4.h"
#include "soulhook/soul-dll.h"
#include "soulhook/soul-gfx.h"

#include "platform/platform.h"

#include "util/dprintf.h"
#include "util/env.h"

static HMODULE soul_hook_mod;
static process_entry_t soul_startup;
static struct soul_hook_config soul_hook_cfg;

static DWORD CALLBACK soul_pre_startup(void)
{
    HRESULT hr;
    bool is_terminal;

    dprintf("--- Begin soul_pre_startup ---\n");

    /* Load config */

    soul_hook_config_load(&soul_hook_cfg, get_config_path());

    /* Hook Win32 APIs */

    dvd_hook_init(&soul_hook_cfg.dvd, soul_hook_mod);
    gfx_hook_init(&soul_hook_cfg.gfx);
    touch_screen_hook_init(&soul_hook_cfg.touch, soul_hook_mod);
    serial_hook_init();
    soulgfx_hook_init();

    /* Initialize emulation hooks */

    hr = platform_hook_init(
            &soul_hook_cfg.platform,
            "SDDP",
            "ACA2",
            soul_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    /* Initialize Terminal/Satellite hooks */
    if (strncmp(soul_hook_cfg.platform.nusec.platform_id, "ACA4", 4) == 0) {
        // Terminal
        is_terminal = true;
    } else if (strncmp(soul_hook_cfg.platform.nusec.platform_id, "ACA2", 4) == 0) {
        // Satellite
        is_terminal = false;
    } else {
        // Unknown
        dprintf("Unknown platform ID: %s\n", soul_hook_cfg.platform.nusec.platform_id);
        goto fail;
    }

    dprintf("System: Cabinet Type: %s\n", is_terminal ? "Terminal" : "Satellite");

    // LED: terminal uses COM 3 and satellite uses COM 2
    unsigned int led_port_no[2] = {is_terminal ? 3 : 2, 0};

    // AIME: terminal uses COM 1 and satellite uses COM 3
    unsigned int aime_port_no = is_terminal ? 1 : 3;

    // VFD: terminal uses COM 4 and satellite uses COM 1
    unsigned int vfd_port_no = is_terminal ? 4 : 1;

    hr = soul_dll_init(&soul_hook_cfg.dll, soul_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = sg_reader_hook_init(&soul_hook_cfg.aime, aime_port_no, 3, soul_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = vfd_hook_init(&soul_hook_cfg.vfd, vfd_port_no);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = soul_io4_hook_init(&soul_hook_cfg.io4);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = led15093_hook_init(&soul_hook_cfg.led15093, 
        soul_dll.led_init, soul_dll.led_set_leds, led_port_no);

    if (FAILED(hr)) {
        goto fail;
    }

    /* Initialize debug helpers */

    spike_hook_init(get_config_path());

    dprintf("---  End  soul_pre_startup ---\n");

    /* Jump to EXE start address */

    return soul_startup();

fail:
    ExitProcess(EXIT_FAILURE);
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx)
{
    HRESULT hr;

    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    soul_hook_mod = mod;

    hr = process_hijack_startup(soul_pre_startup, &soul_startup);

    if (!SUCCEEDED(hr)) {
        dprintf("Failed to hijack process startup: %x\n", (int) hr);
    }

    return SUCCEEDED(hr);
}
