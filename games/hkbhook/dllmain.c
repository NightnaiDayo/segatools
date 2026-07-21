/*
    "Chrono Regalia" (hakobune) hook

    Devices

    USB:    837-15257-01 "Type 4" I/O Board

    [Satellite]

    COM1:   200-6275 VFD GP1232A02A FUTABA board
    COM2:   837-15093-06 LED Controller Board
    COM3:   837-15396 "Gen 3" Aime Reader

    [Terminal]

    COM1:   837-15396 "Gen 3" Aime Reader
    COM3:   837-15093-06 LED Controller Board
    COM4:   200-6275 VFD GP1232A02A FUTABA board
*/

#include <windows.h>

#include <stdlib.h>

#include "board/led15093.h"
#include "board/sg-reader.h"
#include "board/vfd.h"

#include "hook/process.h"

#include "hooklib/dvd.h"
#include "hooklib/touch.h"
#include "hooklib/serial.h"
#include "hooklib/spike.h"

#include "hkbhook/amptw.h"
#include "hkbhook/config.h"
#include "hkbhook/io4.h"
#include "hkbhook/hkb-dll.h"

#include "platform/platform.h"
#include "unityhook/hook.h"

#include "util/dprintf.h"
#include "util/env.h"

static HMODULE hkb_hook_mod;
static process_entry_t hkb_startup;
static struct hkb_hook_config hkb_hook_cfg;

void unity_hook_callback(HMODULE module, const wchar_t* p) {
    if (wcscmp(p, L"amptw.dll") == 0) {
        if (hkb_hook_cfg.touch.enable) {
            touch_hook_insert_hooks(module);
            amptw_apply_hooks(module);
        }
    }
}

static DWORD CALLBACK hkb_pre_startup(void) {
    HRESULT hr;
    bool is_terminal;

    dprintf("--- Begin hkb_pre_startup ---\n");

    /* Load config */

    hkb_hook_config_load(&hkb_hook_cfg, get_config_path());

    /* Hook Win32 APIs */

    dvd_hook_init(&hkb_hook_cfg.dvd, hkb_hook_mod);
    touch_screen_hook_init(&hkb_hook_cfg.touch, hkb_hook_mod);
    touch_hook_insert_hooks(hkb_hook_mod);
    serial_hook_init();

    /* Initialize Terminal/Satellite hooks */
    if (strncmp(hkb_hook_cfg.platform.nusec.platform_id, "ACA4", 4) == 0) {
        // Terminal
        is_terminal = true;
    } else if (strncmp(hkb_hook_cfg.platform.nusec.platform_id, "ACA2", 4) == 0) {
        // Satellite
        is_terminal = false;
    } else {
        // Unknown
        dprintf("Unknown platform ID: %s\n", hkb_hook_cfg.platform.nusec.platform_id);
        goto fail;
    }

    dprintf("HKB: Cabinet Type: %s\n", is_terminal ? "Terminal" : "Satellite");

    /* Hook external DLL APIs */

    amptw_hook_init(&hkb_hook_cfg.touch, hkb_hook_mod);

    /* Initialize emulation hooks */

    hr = platform_hook_init(&hkb_hook_cfg.platform, "SDEC", is_terminal ? "ACA4" : "ACA2", hkb_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = sg_reader_hook_init(&hkb_hook_cfg.aime, is_terminal ? 1 : 3, 3, hkb_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = hkb_dll_init(&hkb_hook_cfg.dll, hkb_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = vfd_hook_init(&hkb_hook_cfg.vfd, is_terminal ? 4 : 1);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = hkb_io4_hook_init(&hkb_hook_cfg.io4, is_terminal);

    if (FAILED(hr)) {
        goto fail;
    }

    unsigned int led_port_no[2] = {
        is_terminal ? 3 : 2,
        0
    };
    hr = led15093_hook_init(&hkb_hook_cfg.led15093, hkb_dll.led_init, hkb_dll.led_set_leds, led_port_no);

    if (FAILED(hr)) {
        goto fail;
    }

    unity_hook_init(&hkb_hook_cfg.unity, hkb_hook_mod, unity_hook_callback);

    /* Initialize debug helpers */

    spike_hook_init(get_config_path());

    dprintf("---  End  hkb_pre_startup ---\n");

    /* Jump to EXE start address */

    return hkb_startup();

fail:
    ExitProcess(EXIT_FAILURE);
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void* ctx) {
    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    hkb_hook_mod = mod;

    HRESULT hr = process_hijack_startup(hkb_pre_startup, &hkb_startup);

    if (!SUCCEEDED(hr)) {
        dprintf("Failed to hijack process startup: %x\n", (int)hr);
    }

    return SUCCEEDED(hr);
}
