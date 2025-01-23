/*
    "maimai DX" (mai2) hook

    Devices

    USB:    837-15257-01 "Type 4" I/O Board
    USB:    2 * 601-13216 USB "QR Code" Camera (SDEZ2, SDEZ3)
    USB:    601-13249 USB "Player" Camera (SDEZ1)
    USB:    837-15067-02 IC BD USB to Serial 232
            connected to
            837-15070-04 LED Board Controller (COM21)
            837-15070-04 LED Board Controller (COM23)

    COM1:   837-15396 "Gen 3" Aime Reader
    COM2:   200-6275 VFD GP1232A02A FUTABA Board
    COM3:   509-6483 Touch Panel Controller
    COM4:   509-6483 Touch Panel Controller
*/

#include <windows.h>

#include <stdlib.h>

#include "board/io4.h"
#include "board/sg-reader.h"
#include "board/vfd.h"

#include "hook/process.h"
#include "hook/table.h"
#include "hook/iohook.h"

#include "hooklib/serial.h"
#include "hooklib/spike.h"

#include "mai2hook/config.h"
#include "mai2hook/io4.h"
#include "mai2hook/mai2-dll.h"

#include "platform/platform.h"

#include "unityhook/hook.h"

#include "util/dprintf.h"
#include "util/env.h"

static HMODULE mai2_hook_mod;
static process_entry_t mai2_startup;
static struct mai2_hook_config mai2_hook_cfg;

/* This hook is based on mu3hook, with leaked mai2hook i/o codes. */

static DWORD CALLBACK mai2_pre_startup(void)
{
    HRESULT hr;

    dprintf("--- Begin mai2_pre_startup ---\n");

    /* Load config */

    mai2_hook_config_load(&mai2_hook_cfg, get_config_path());

    /* Hook Win32 APIs */

    dvd_hook_init(&mai2_hook_cfg.dvd, mai2_hook_mod);
    serial_hook_init();

    /* Initialize emulation hooks */

    hr = platform_hook_init(
        &mai2_hook_cfg.platform,
        "SDEZ",
        "ACA1",
        mai2_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    /* Initialize DLLs */

    hr = mai2_dll_init(&mai2_hook_cfg.dll, mai2_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    // Touch Panel uses COM3 and COM4

    hr = touch_hook_init(&mai2_hook_cfg.touch);

    if (FAILED(hr)) {
        goto fail;
    }

    // LED board uses COM21 and COM23
    hr = led15070_hook_init(&mai2_hook_cfg.led15070,
        mai2_dll.led_init,
        mai2_dll.led_set_fet_output,
        mai2_dll.led_dc_update,
        mai2_dll.led_gs_update,
        21, 2);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = sg_reader_hook_init(&mai2_hook_cfg.aime, 1, 3, mai2_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = vfd_hook_init(&mai2_hook_cfg.vfd, 2);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = mai2_io4_hook_init(&mai2_hook_cfg.io4);

    if (FAILED(hr)) {
        goto fail;
    }

    /* Initialize Unity native plugin DLL hooks

       There seems to be an issue with other DLL hooks if `LoadLibraryW` is
       hooked earlier in the `mai2hook` initialization. */

    unity_hook_init(&mai2_hook_cfg.unity, mai2_hook_mod, NULL);

    /* Initialize debug helpers */

    spike_hook_init(get_config_path());

    dprintf("---  End  mai2_pre_startup ---\n");

    /* Jump to EXE start address */

    return mai2_startup();

fail:
    ExitProcess(EXIT_FAILURE);
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx)
{
    HRESULT hr;

    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    mai2_hook_mod = mod;

    hr = process_hijack_startup(mai2_pre_startup, &mai2_startup);

    if (!SUCCEEDED(hr)) {
        dprintf("Failed to hijack process startup: %x\n", (int) hr);
    }

    return SUCCEEDED(hr);
}
