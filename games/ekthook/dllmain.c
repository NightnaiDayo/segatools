#include <windows.h>


#include "ekt-dll.h"
#include "board/sg-reader.h"
#include "board/led15093.h"

#include "hook/process.h"

#include "hooklib/serial.h"
#include "hooklib/spike.h"

#include "ekthook/config.h"
#include "ekthook/io4.h"
#include "ekthook/y3.h"
#include "hook/iohook.h"

#include "platform/platform.h"
#include "unityhook/hook.h"

#include "util/dprintf.h"
#include "util/env.h"

/*
 * Eiketsu Taisen
 *
 * IP: 192.168.189.0
 *
 * SATELLITE: Model Type 1
 *
 * COM2: LED
 * COM3: AIME
 * COM3: "TM LED"
 * COM4: FPR / Y3
 *
 * TERMINAL: Model Type 2
 *
 * COM1: Aime
 * COM3: LED
 *
 */

static HMODULE ekt_hook_mod;
static process_entry_t ekt_startup;
static struct ekt_hook_config ekt_hook_cfg;

static const wchar_t *target_modules[] = {
    L"Y3CodeReaderNE.dll",
};

static const size_t target_modules_len = _countof(target_modules);

void unity_hook_callback(HMODULE hmodule, const wchar_t* p) {
    dprintf("Unity: Hook callback: %ls\n", p);

    for (size_t i = 0; i < target_modules_len; i++) {
        if (_wcsicmp(p, target_modules[i]) == 0) {
            serial_hook_apply_hooks(hmodule);
            iohook_apply_hooks(hmodule);
        }
    }
}

static DWORD CALLBACK ekt_pre_startup(void)
{
    HMODULE d3dc;
    HMODULE dbghelp;
    HRESULT hr;
    bool is_terminal;

    dprintf("--- Begin ekt_pre_startup ---\n");

    /* Pin the D3D shader compiler. This makes startup much faster. */

    d3dc = LoadLibraryA("d3dcompiler_43.dll");

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

    /* Load config */

    ekt_hook_config_load(&ekt_hook_cfg, get_config_path());

    /* Hook Win32 APIs */
    serial_hook_init();

    /* Initialize emulation hooks */

    // Terminal = AAV1, Satellite = AAV2
    hr = platform_hook_init(
            &ekt_hook_cfg.platform,
            "SDGY",
            "AAV1",
            ekt_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    /* Initialize Terminal/Satellite hooks */
    if (strncmp(ekt_hook_cfg.platform.nusec.platform_id, "AAV1", 4) == 0) {
        // Terminal
        is_terminal = true;
    } else if (strncmp(ekt_hook_cfg.platform.nusec.platform_id, "AAV2", 4) == 0) {
        // Satellite
        is_terminal = false;
    } else {
        // Unknown
        dprintf("Unknown platform ID: %s\n", ekt_hook_cfg.platform.nusec.platform_id);
        goto fail;
    }

    if (FAILED(hr)) {
        goto fail;
    }

    hr = ekt_dll_init(&ekt_hook_cfg.dll, ekt_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = ekt_io4_hook_init(&ekt_hook_cfg.io4, is_terminal);

    if (FAILED(hr)) {
        goto fail;
    }
    
    unsigned int led_port_no[2] = {is_terminal ? 3 : 2, 0};
    hr = led15093_hook_init(&ekt_hook_cfg.led15093, 
        ekt_dll.led_init, ekt_dll.led_set_leds, led_port_no);

    if (FAILED(hr)) {
        goto fail;
    }


    hr = sg_reader_hook_init(&ekt_hook_cfg.aime, is_terminal ? 1 : 3, ekt_hook_cfg.aime.gen, ekt_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    unity_hook_init(&ekt_hook_cfg.unity, ekt_hook_mod, unity_hook_callback);

    y3_hook_init(&ekt_hook_cfg.y3, ekt_hook_mod);

    /* Initialize debug helpers */

    spike_hook_init(get_config_path());

    dprintf("---  End  ekt_pre_startup ---\n");

    /* Jump to EXE start address */

    return ekt_startup();

fail:
    ExitProcess(EXIT_FAILURE);
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx)
{
    HRESULT hr;

    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    ekt_hook_mod = mod;

    hr = process_hijack_startup(ekt_pre_startup, &ekt_startup);

    if (!SUCCEEDED(hr)) {
        dprintf("Failed to hijack process startup: %x\n", (int) hr);
    }

    return SUCCEEDED(hr);
}
