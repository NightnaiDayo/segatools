/*
    "Fate Grand/Order Arcade" (fgo) hook

    Devices

    USB:    837-15257 "Type 4" I/O Board
    USB:    838-15405 "WinTouch" Controller Board
    USB:    630-00008 Sinfonia CHC-C330 Printer
    USB:    837-14509-02 USB-SER I/F BD Mini-B FTDI Board
            connected to
            837-15093-06 LED Controller Board
    COM1:   200-6275 VFD GP1232A02A FUTABA Board
    COM2:   837-15345 RFID Deck Reader Noard
    COM3:   837-15396 "Gen 3" Aime Reader
    COM4:   837-15347 RFID Reader/Writer Board (inside the printer)
*/

#include <windows.h>

#include <stdlib.h>

#include "board/io4.h"
#include "board/led15093.h"
#include "board/sg-reader.h"
#include "board/vfd.h"

#include "hook/process.h"

#include "hooklib/dll.h"
#include "hooklib/dvd.h"
#include "hooklib/touch.h"
#include "hooklib/printer.h"
#include "hooklib/createprocess.h"
#include "hooklib/serial.h"
#include "hooklib/spike.h"

#include "gfxhook/gfx.h"

#include "fgohook/config.h"
#include "fgohook/io4.h"
#include "fgohook/fgo-dll.h"
#include "fgohook/deck.h"

#include "platform/platform.h"

#include "util/dprintf.h"
#include "util/env.h"

static HMODULE fgo_hook_mod;
static process_entry_t fgo_startup;
static struct fgo_hook_config fgo_hook_cfg;

static DWORD CALLBACK fgo_pre_startup(void)
{
    HRESULT hr;
    HMODULE dbghelp;

    dprintf("--- Begin fgo_pre_startup ---\n");

    /* Pin dbghelp so the path hooks apply to it. */

    dbghelp = LoadLibraryW(L"dbghelp.dll");

    if (dbghelp != NULL) {
        dprintf("Pinned debug helper library, hMod=%p\n", dbghelp);
    }
    else {
        dprintf("Failed to load debug helper library!\n");
    }

    /* Load config */

    fgo_hook_config_load(&fgo_hook_cfg, get_config_path());

    /* Hook Win32 APIs */

    dvd_hook_init(&fgo_hook_cfg.dvd, fgo_hook_mod);
    gfx_hook_init(&fgo_hook_cfg.gfx);
    touch_screen_hook_init(&fgo_hook_cfg.touch, fgo_hook_mod);
    serial_hook_init();

    /* Hook external DLL APIs */

    printer_hook_init(&fgo_hook_cfg.printer, 4, fgo_hook_mod);
    if (fgo_hook_cfg.printer.enable) {
        dll_hook_push(fgo_hook_mod, L"C330Ausb.dll");
        dll_hook_push(fgo_hook_mod, L"C330AFWDLusb.dll");
    }

    /* Initialize emulation hooks */

    hr = platform_hook_init(
            &fgo_hook_cfg.platform,
            "SDEJ",
            "ACA1",
            fgo_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = sg_reader_hook_init(&fgo_hook_cfg.aime, 3, 3, fgo_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = vfd_hook_init(&fgo_hook_cfg.vfd, 1);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = fgo_dll_init(&fgo_hook_cfg.dll, fgo_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = fgo_io4_hook_init(&fgo_hook_cfg.io4);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = deck_hook_init(&fgo_hook_cfg.deck, 2);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = ftdi_hook_init(&fgo_hook_cfg.ftdi, 17);

    if (FAILED(hr)) {
        goto fail;
    }

    unsigned int led_port_no[2] = {17, 0};
    hr = led15093_hook_init(&fgo_hook_cfg.led15093, 
        fgo_dll.led_init, fgo_dll.led_set_leds, led_port_no);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = createprocess_push_hook_a("am/amdaemon.exe", "inject -d -k fgohook.dll ", "", false);

    if (FAILED(hr)) {
        goto fail;
    }

    /* Initialize debug helpers */

    spike_hook_init(get_config_path());

    dprintf("---  End  fgo_pre_startup ---\n");

    /* Jump to EXE start address */

    return fgo_startup();

fail:
    ExitProcess(EXIT_FAILURE);
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx)
{
    HRESULT hr;

    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    fgo_hook_mod = mod;

    hr = process_hijack_startup(fgo_pre_startup, &fgo_startup);

    if (!SUCCEEDED(hr)) {
        dprintf("Failed to hijack process startup: %x\n", (int) hr);
    }

    return SUCCEEDED(hr);
}
