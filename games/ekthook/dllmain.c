/*
    "Eiketsu Taisen" (ekt) hook

    Devices

    USB:    837-15257-01 "Type 4" I/O Board

    [Satellite]

    USB:    630-00011 G-Printec CX-7000 Printer
    COM2:   837-15093-06 LED Controller Board
    COM3:   837-15396 "Gen 3" Aime Reader
    COM4:   601-13160-01 "Flat Panel Reader" Y3CR BD SIE F720MM Board

    [Terminal]

    COM1:   837-15396 "Gen 3" Aime Reader
    COM3:   837-15093-06 LED Controller Board
*/

#include <assert.h>
#include <shlwapi.h>
#include <stdio.h>
#include <windows.h>

#include <stdlib.h>

#include "ekt-dll.h"
#include "board/sg-reader.h"
#include "board/led15093.h"

#include "hook/process.h"
#include "hook/iohook.h"

#include "hooklib/serial.h"
#include "hooklib/spike.h"

#include "ekthook/config.h"
#include "ekthook/io4.h"
#include "hooklib/createprocess.h"
#include "hooklib/printer_cx.h"

#include "platform/platform.h"

#include "unityhook/hook.h"

#include "util/dprintf.h"
#include "util/env.h"
#include "hooklib/y3-dll.h"
#include "hooklib/y3.h"
#include "util/lib.h"

static HMODULE ekt_hook_mod;
static process_entry_t ekt_startup;
static struct ekt_hook_config ekt_hook_cfg;

static void unity_hook_callback(HMODULE hmodule, const wchar_t* p) {
    netenv_hook_apply_hooks(hmodule);
    createprocess_hook_apply_hooks(hmodule);
}

static void check_and_display_warning(void) {
    wchar_t* module_path;
    wchar_t* file_name;

    module_path = module_file_name(NULL);

    if (module_path != NULL) {
        file_name = PathFindFileNameW(module_path);

        free(module_path);
        module_path = NULL;

        _wcslwr(file_name);

        if (wcsstr(file_name, L"ekt.exe") != NULL) {
            wchar_t recording_flag_path[MAX_PATH];
            PathCombineW(recording_flag_path, ekt_hook_cfg.platform.vfs.appdata, L"recording_warning_seen");
            if (!PathFileExistsW(recording_flag_path)) {
                if (MessageBoxW(
                        NULL,
                        L"This game has an ability during battle to record and upload your entire desktop content (including other windows, task bar, browsers, etc.)\n\nMake sure you trust the server you are playing on.\n\nThis message will not be shown again.",
                        L"Segatools Privacy Warning", MB_ICONWARNING | MB_OKCANCEL) != IDOK) {
                    ExitProcess(0);
                }
                CreateFileW(recording_flag_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW, 0, NULL);
            }
        }
    }
}

static DWORD CALLBACK ekt_pre_startup(void)
{
    HRESULT hr;
    bool is_terminal;

    dprintf("--- Begin ekt_pre_startup ---\n");

    /* Load config */

    ekt_hook_config_load(&ekt_hook_cfg, get_config_path());

    /* Hook Win32 APIs */

    dvd_hook_init(&ekt_hook_cfg.dvd, ekt_hook_mod);
    serial_hook_init();

    createprocess_push_hook_w(L"fsutil", L"", L"", true, true);

    /* Hook external DLL APIs */

    hr = y3_hook_init(&ekt_hook_cfg.y3, ekt_hook_mod, get_config_path());

    if (FAILED(hr)) {
        goto fail;
    }

    printer_cx_hook_init(&ekt_hook_cfg.printer, ekt_hook_mod);

    /* Initialize emulation hooks */

    hr = platform_hook_init(
            &ekt_hook_cfg.platform,
            "SDGY",
            "ACA1",
            ekt_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    /* Initialize Terminal/Satellite hooks */
    if (strncmp(ekt_hook_cfg.platform.nusec.platform_id, "ACA1", 4) == 0) {
        // Terminal
        is_terminal = true;
    } else if (strncmp(ekt_hook_cfg.platform.nusec.platform_id, "ACA2", 4) == 0) {
        // Satellite
        is_terminal = false;
    } else {
        // Unknown
        dprintf("Unknown platform ID: %s\n", ekt_hook_cfg.platform.nusec.platform_id);
        goto fail;
    }

    dprintf("System: Cabinet Type: %s\n", is_terminal ? "Terminal" : "Satellite");

    // LED: terminal uses COM 3 and satellite use COM 2
    unsigned int led_port_no[2] = {is_terminal ? 3 : 2, 0};

    // AIME: terminal uses COM 1 and satellite use COM 3
    unsigned int aime_port_no = is_terminal ? 1 : 3;

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
    
    hr = led15093_hook_init(&ekt_hook_cfg.led15093, 
        ekt_dll.led_init, ekt_dll.led_set_leds, led_port_no);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = sg_reader_hook_init(&ekt_hook_cfg.aime, aime_port_no, 3,
        ekt_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    /* Initialize Unity native plugin DLL hooks

       There seems to be an issue with other DLL hooks if `LoadLibraryW` is
       hooked earlier in the `ekthook` initialization. */

    unity_hook_init(&ekt_hook_cfg.unity, ekt_hook_mod, unity_hook_callback);

    /* Initialize debug helpers */

    spike_hook_init(get_config_path());

    dprintf("---  End  ekt_pre_startup ---\n");

    /* Recording warning */
    check_and_display_warning();

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
