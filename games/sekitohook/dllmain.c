/*
    "Sangokushi Taisen" (sekito) hook

    Devices

    USB:    837-14572 "Type 3" I/O Board
    COM12:  837-15084 "Gen 2" Aime Reader

    [Satellite]

    USB:    Sinfonia CHC-C320 Printer
    COM1:   837-15093-06 LED Controller Board
    COM10:  601-13160-01 "Flat Panel Reader" Y3CR BD SIE F720MM Board
    COM11:  Printer Camera

    [Terminal]

    COM1:   837-15084 "Gen 2" Aime Reader
    COM11:  837-15093-06 LED Controller Board
*/

#include <windows.h>

#include <stdlib.h>

#include "amex/amex.h"
#include "board/sg-reader.h"
#include "board/led15093.h"
#include "board/sg-reader-queue.h"

#include "gfxhook/d3d9.h"
#include "gfxhook/gfx.h"

#include "hook/process.h"
#include "hook/iohook.h"

#include "hooklib/dll.h"
#include "hooklib/serial.h"
#include "hooklib/spike.h"
#include "hooklib/y3.h"

#include "sekitohook/config.h"
#include "sekitohook/jvs.h"

#include "platform/platform.h"

#include "util/dprintf.h"
#include "util/env.h"
#include "util/fg-detect.h"

static HMODULE sekito_hook_mod;
static process_entry_t sekito_startup;
static struct sekito_hook_config sekito_hook_cfg;

static DWORD CALLBACK sekito_pre_startup(void)
{
    HMODULE dbghelp;
    HRESULT hr;
    bool is_terminal;

    dprintf("--- Begin sekito_pre_startup ---\n");

    /* Pin dbghelp so the path hooks apply to it. */

    dbghelp = LoadLibraryW(L"dbghelp.dll");

    if (dbghelp != NULL) {
        dprintf("Pinned debug helper library, hMod=%p\n", dbghelp);
    } else {
        dprintf("Failed to load debug helper library!\n");
    }

    /* Load config */

    sekito_hook_config_load(&sekito_hook_cfg, get_config_path());

    /* Hook Win32 APIs */

    dvd_hook_init(&sekito_hook_cfg.dvd, sekito_hook_mod);
    gfx_hook_init(&sekito_hook_cfg.gfx);
    gfx_d3d9_hook_init(&sekito_hook_cfg.gfx, sekito_hook_mod);
    serial_hook_init();
    sekito_io_init();

    /* Hook external DLL APIs */

    hr = y3_hook_init(&sekito_hook_cfg.y3, sekito_hook_mod, get_config_path());

    if (FAILED(hr)) {
        goto fail;
    }

    printer_chc_hook_init(&sekito_hook_cfg.printer, 0, sekito_hook_mod);
    if (sekito_hook_cfg.printer.enable) {
        dll_hook_push(sekito_hook_mod, L"C320Ausb.dll");
        dll_hook_push(sekito_hook_mod, L"C320AFWDLusb.dll");
    }

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

    dprintf("System: Cabinet Type: %s\n", is_terminal ? "Terminal" : "Satellite");

    // LED: terminal uses COM 11 and satellite use COM 1
    unsigned int led_port_no[2] = {is_terminal ? 11 : 1, 0};

    hr = sekito_dll_init(&sekito_hook_cfg.dll, sekito_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }
    
    hr = led15093_hook_init(&sekito_hook_cfg.led15093,
        sekito_dll.led_init, sekito_dll.led_set_leds, led_port_no);

    if (FAILED(hr)) {
        goto fail;
    }

    if (is_terminal) {
        hr = sg_reader_queue_hook_init(&sekito_hook_cfg.aime_queue, 12, 2,
            sekito_hook_mod);

        if (FAILED(hr)) {
            goto fail;
        }

        hr = sg_reader_hook_init(&sekito_hook_cfg.aime, 1, 2,
            sekito_hook_mod);

        if (FAILED(hr)) {
            goto fail;
        }
    } else {
        hr = sg_reader_hook_init(&sekito_hook_cfg.aime, 12, 2,
            sekito_hook_mod);

        if (FAILED(hr)) {
            goto fail;
        }
    }

    sekito_jvs_set_terminal(is_terminal);
    hr = amex_hook_init(&sekito_hook_cfg.amex, sekito_jvs_init);

    if (FAILED(hr)) {
        goto fail;
    }

    if (sekito_hook_cfg.amex.jvs.enable && sekito_hook_cfg.amex.jvs.foreground) {
        fgdet_init(L"teaGfx DirectX Release", false);
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
