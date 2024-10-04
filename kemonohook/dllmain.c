#include <windows.h>

#include "board/io4.h"
#include "board/sg-reader.h"
#include "board/vfd.h"

#include "hook/process.h"
#include "hook/table.h"
#include "hook/iohook.h"

#include "hooklib/printer.h"
#include "hooklib/serial.h"
#include "hooklib/spike.h"

#include "kemonohook/config.h"
#include "kemonohook/hooks.h"
#include "kemonohook/jvs.h"
#include "kemonohook/kemono-dll.h"

#include "platform/platform.h"

#include "unityhook/hook.h"

#include "util/dprintf.h"

static HMODULE kemono_hook_mod;
static process_entry_t kemono_startup;
static struct kemono_hook_config kemono_hook_cfg;

static DWORD CALLBACK kemono_pre_startup(void) {
    HRESULT hr;

    dprintf("--- Begin kemono_pre_startup ---\n");

    /* Load config */

    kemono_hook_cfg.aime.dll.path64 = true;
    kemono_hook_config_load(&kemono_hook_cfg, L".\\segatools.ini");

    /* Hook Win32 APIs */

    dvd_hook_init(&kemono_hook_cfg.dvd, kemono_hook_mod);
    serial_hook_init();

    // 2.02 does not call printer update functions
    uint16_t ret;
    fwdlusb_updateFirmware_main(1, "UnityApp\\Parade_Data\\StreamingAssets\\Printer\\E0223100-014E-C300-MAINAPP.BIN", &ret);
    if (ret != 0){
        goto fail;
    }
    fwdlusb_updateFirmware_dsp(2, "UnityApp\\Parade_Data\\StreamingAssets\\Printer\\E0223200-0101-C300-DSPAPP.BIN", &ret);
    if (ret != 0){
        goto fail;
    }
    fwdlusb_updateFirmware_param(3, "UnityApp\\Parade_Data\\StreamingAssets\\Printer\\D0460700-0101-C300-PARAM.BIN", &ret);
    if (ret != 0){
        goto fail;
    }

    printer_hook_init(&kemono_hook_cfg.printer, 0, kemono_hook_mod);
    printer_set_dimensions(720, 1028); // printer doesn't call setimageformat

    /* Initialize emulation hooks */

    hr = platform_hook_init(
            &kemono_hook_cfg.platform,
            "SDFL",
            "AAW1",
            kemono_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = sg_reader_hook_init(&kemono_hook_cfg.aime, 1, 1, kemono_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = kemono_dll_init(&kemono_hook_cfg.dll, kemono_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = amex_hook_init(&kemono_hook_cfg.amex, kemono_jvs_init);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = led15093_hook_init(&kemono_hook_cfg.led15093, kemono_dll.led_init, kemono_dll.led_set_leds, 10, 1, 1, 2);

    if (FAILED(hr)) {
        goto fail;
    }

    kemono_extra_hooks_init();

    /* Initialize Unity native plugin DLL hooks

       There seems to be an issue with other DLL hooks if `LoadLibraryW` is
       hooked earlier in the `kemonohook` initialization. */

    unity_hook_init(&kemono_hook_cfg.unity, kemono_hook_mod, kemono_extra_hooks_load);

    /* Initialize debug helpers */

    spike_hook_init(L".\\segatools.ini");

    dprintf("---  End  kemono_pre_startup ---\n");

    /* Jump to EXE start address */

    return kemono_startup();

    fail:
    ExitProcess(EXIT_FAILURE);
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx) {
    HRESULT hr;

    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    kemono_hook_mod = mod;

    hr = process_hijack_startup(kemono_pre_startup, &kemono_startup);

    if (!SUCCEEDED(hr)) {
        dprintf("Failed to hijack process startup: %x\n", (int) hr);
    }

    return SUCCEEDED(hr);
}
