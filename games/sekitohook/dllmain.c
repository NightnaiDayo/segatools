/*
    "Sangokushi Taisen" (sekito) hook

    Devices

    USB:    837-15257-01 "Type 4" I/O Board
    COM12:  837-15396 "Gen 3" Aime Reader

    [Satellite]

    USB:    Printer
    COM1:   837-15093-06 LED Controller Board
    COM11:  Printer Camera

    [Terminal]

    COM1:   837-15396 "Gen 3" Aime Reader
    COM3:   837-15093-06 LED Controller Board
*/

#include <windows.h>

#include <stdlib.h>

#include "sekito-dll.h"
#include "board/sg-reader.h"
#include "board/led15093.h"

#include "hook/process.h"
#include "hook/iohook.h"

#include "hooklib/serial.h"
#include "hooklib/spike.h"

#include "sekitohook/config.h"
#include "sekitohook/io4.h"
#include "hooklib/printer_cx.h"

#include "platform/platform.h"

#include "unityhook/hook.h"

#include "util/dprintf.h"
#include "util/env.h"
#include "hooklib/y3-dll.h"
#include "hooklib/y3.h"

static HMODULE sekito_hook_mod;
static process_entry_t sekito_startup;
static struct sekito_hook_config sekito_hook_cfg;

static void unity_hook_callback(HMODULE hmodule, const wchar_t* p) {
    netenv_hook_apply_hooks(hmodule);
}

static DWORD CALLBACK sekito_pre_startup(void)
{
    HRESULT hr;
    bool is_terminal;

    dprintf("--- Begin sekito_pre_startup ---\n");

    /* Load config */

    sekito_hook_config_load(&sekito_hook_cfg, get_config_path());

    /* Hook Win32 APIs */

    dvd_hook_init(&sekito_hook_cfg.dvd, sekito_hook_mod);
    serial_hook_init();

    /* Hook external DLL APIs */

    hr = y3_hook_init(&sekito_hook_cfg.y3, sekito_hook_mod, get_config_path());

    if (FAILED(hr)) {
        goto fail;
    }

    printer_chc_hook_init(&sekito_hook_cfg.printer, 0, sekito_hook_mod);

    /* Initialize emulation hooks */

    hr = platform_hook_init(
            &sekito_hook_cfg.platform,
            "SDDD",
            "AAV2",
            sekito_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    /* Initialize Terminal/Satellite hooks */
    if (strncmp(sekito_hook_cfg.platform.nusec.platform_id, "AAV1", 4) == 0) {
        // Terminal
        is_terminal = true;
    } else if (strncmp(sekito_hook_cfg.platform.nusec.platform_id, "AAV2", 4) == 0) {
        // Satellite
        is_terminal = false;
    } else {
        // Unknown
        dprintf("Unknown platform ID: %s\n", sekito_hook_cfg.platform.nusec.platform_id);
        goto fail;
    }

    // LED: terminal uses COM 3 and satellite use COM 2
    unsigned int led_port_no[2] = {is_terminal ? 3 : 2, 0};

    hr = sekito_dll_init(&sekito_hook_cfg.dll, sekito_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = sekito_io4_hook_init(&sekito_hook_cfg.io4, is_terminal);

    if (FAILED(hr)) {
        goto fail;
    }
    
    hr = led15093_hook_init(&sekito_hook_cfg.led15093,
        sekito_dll.led_init, sekito_dll.led_set_leds, led_port_no);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = sg_reader_hook_init(&sekito_hook_cfg.aime, 12, 3,
        sekito_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    if (is_terminal) {

        hr = sg_reader_hook_init(&sekito_hook_cfg.aime, 1, 3,
            sekito_hook_mod);

        if (FAILED(hr)) {
            goto fail;
        }

    }

    /* Initialize debug helpers */

    spike_hook_init(get_config_path());

    dprintf("---  End  sekito_pre_startup ---\n");

    /* Jump to EXE start address */

    return sekito_startup();

fail:
    ExitProcess(EXIT_FAILURE);
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx)
{
    HRESULT hr;

    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    sekito_hook_mod = mod;

    hr = process_hijack_startup(sekito_pre_startup, &sekito_startup);

    if (!SUCCEEDED(hr)) {
        dprintf("Failed to hijack process startup: %x\n", (int) hr);
    }

    return SUCCEEDED(hr);
}
