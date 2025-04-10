/*
    "Wonderland Wars" (carol*) hook

    Devices:

    JVS:    837-14572 "Type 3" I/O Board

    [Satellite]

    USB:    "WinTouch" Controller Board
                ^ (DIPSW2 ON, Version 5.xx.xx or above)
    COM1:   3M Touch Systems 78-0011-2353-4 Touch Controller Board
                ^ (DIPSW2 OFF)
    COM10:  TN32MSEC003S "Gen 1" Aime Reader
            OR
            837-15286 "Gen 2" Aime Reader
                ^ (Version 1.6x.xx or above)
    COM11:  837-15070-02 LED Controller Board
    COM12:  837-15312 Pen Controller I/O Board

    [Terminal]

    COM10:  837-15286 "Gen 2" Aime Reader

    *: SEGA's abbreviation for Lewis Carroll, author of Alice's Adventures in
    Wonderland.
*/

#include <windows.h>

#include <stdlib.h>

#include "amex/amex.h"
#include "gfxhook/gfx.h"
#include "gfxhook/d3d9.h"

#include "board/sg-reader.h"

#include "carolhook/config.h"
#include "carolhook/carol-dll.h"
#include "carolhook/jvs.h"
#include "carolhook/touch.h"
#include "carolhook/ledbd.h"
#include "carolhook/controlbd.h"

#include "hook/process.h"

#include "hooklib/serial.h"
#include "hooklib/spike.h"
#include "hooklib/createprocess.h"
#include "hooklib/cursor.h"

#include "platform/platform.h"

#include "util/dprintf.h"
#include "util/env.h"

static HMODULE carol_hook_mod;
static process_entry_t carol_startup;
static struct carol_hook_config carol_hook_cfg;

/*
COM Layout
01: Touchscreen
10: Aime reader
11: LED board
12: Control Board
*/

static DWORD CALLBACK carol_pre_startup(void)
{
    HRESULT hr;
    HMODULE d3dc;
    HMODULE dbghelp;

    dprintf("--- Begin carol_pre_startup ---\n");
    if ( !SetProcessDPIAware() )
        dprintf("Failed to set process DPI awareness level!\n");
    
    /* Pin the D3D shader compiler. This makes startup much faster. */

    d3dc = LoadLibraryW(L"D3DCompiler_43.dll");

    if (d3dc != NULL) {
        dprintf("Pinned shader compiler, hMod=%p\n", d3dc);
    } else {
        dprintf("Failed to load shader compiler!\n");
    }

    /* Pin dbghelp so the path hooks apply to it. */

    dbghelp = LoadLibraryW(L"dbghelp.dll");

    if (dbghelp != NULL) {
        dprintf("Pinned debug helper library, hMod=%p\n", dbghelp);
    } else {
        dprintf("Failed to load debug helper library!\n");
    }

    cursor_hook_init();

    /* Config load */

    carol_hook_config_load(&carol_hook_cfg, get_config_path());

    /* Hook Win32 APIs */

    serial_hook_init();

    /* Initialize emulation hooks */

    hr = platform_hook_init(
            &carol_hook_cfg.platform,
            "SDAP",
            "AAV0",
            carol_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = carol_dll_init(&carol_hook_cfg.dll, carol_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = amex_hook_init(&carol_hook_cfg.amex, carol_jvs_init);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = sg_reader_hook_init(&carol_hook_cfg.aime, 10, 1, carol_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    gfx_hook_init(&carol_hook_cfg.gfx);
    gfx_d3d9_hook_init(&carol_hook_cfg.gfx, carol_hook_mod);    

    hr = touch_hook_init(&carol_hook_cfg.touch);
    
    if (FAILED(hr)) {
        goto fail;
    }

    hr = ledbd_hook_init(&carol_hook_cfg.ledbd);
    
    if (FAILED(hr)) {
        goto fail;
    }

    hr = controlbd_hook_init(&carol_hook_cfg.controlbd);
    
    if (FAILED(hr)) {
        goto fail;
    }
    
    hr = createprocess_push_hook_a(".\\15312firm\\firmupdate_1113.exe", "inject -d -k carolhook.dll ", NULL, false);
    
    if (FAILED(hr)) {
        goto fail;
    }
    /* Initialize debug helpers */

    spike_hook_init(get_config_path());

    dprintf("---  End  carol_pre_startup ---\n");

    /* Jump to EXE start address */

    return carol_startup();

fail:
    ExitProcess(EXIT_FAILURE);
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx)
{
    HRESULT hr;

    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    carol_hook_mod = mod;

    hr = process_hijack_startup(carol_pre_startup, &carol_startup);

    if (!SUCCEEDED(hr)) {
        dprintf("Failed to hijack process startup: %x\n", (int) hr);
    }

    return SUCCEEDED(hr);
}
