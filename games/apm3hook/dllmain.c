/*
    "ALL.Net P-ras MULTI Version 3" (apm3) hook

    Devices

    USB:    837-15257 "Type 4" I/O Board
    COM1:   200-6275 VFD GP1232A02A FUTABA Board
    COM2:   837-15093-06 LED Controller Board
    COM3:   837-15396 "Gen 3" Aime Reader
*/

#include <windows.h>

#include <stdlib.h>

#include "config.h"
#include "io4.h"
#include "mount.h"

#include "hook/iohook.h"
#include "hook/process.h"

#include "hooklib/serial.h"
#include "hooklib/spike.h"

#include "platform/clock.h"
#include "platform/config.h"
#include "platform/nusec.h"
#include "platform/security.h"

#include "unityhook/hook.h"

#include "util/dprintf.h"
#include "util/env.h"

static HMODULE apm3_hook_mod;
static process_entry_t apm3_startup;
static struct apm3_hook_config apm3_hook_cfg;

static const wchar_t *target_modules[] = {
    L"apmled.dll",
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
    security_hook_insert_hooks(hmodule);
    touch_hook_insert_hooks(hmodule);
    // mount_hook_apply_hooks(&apm3_hook_cfg.mount, hmodule);
}

static DWORD CALLBACK apm3_pre_startup(void)
{
    HRESULT hr;

    dprintf("--- Begin %s ---\n", __func__);

    /* Load config */

    apm3_hook_config_load(&apm3_hook_cfg, get_config_path());

    /* Hook Win32 APIs */

    dvd_hook_init(&apm3_hook_cfg.dvd, apm3_hook_mod);
    touch_screen_hook_init(&apm3_hook_cfg.touch, apm3_hook_mod);
    serial_hook_init();

    /* Initialize emulation hooks */

    hr = platform_hook_init(
            &apm3_hook_cfg.platform,
            "SDEM",
            "ACA1",
            apm3_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = sg_reader_hook_init(&apm3_hook_cfg.aime, 3, 3, apm3_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = vfd_hook_init(&apm3_hook_cfg.vfd, 1);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = apm3_dll_init(&apm3_hook_cfg.dll, apm3_hook_mod);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = apm3_io4_hook_init(&apm3_hook_cfg.io4);

    if (FAILED(hr)) {
        goto fail;
    }
    
    unsigned int led_port_no[2] = {2, 0};
    hr = led15093_hook_init(&apm3_hook_cfg.led15093,
        apm3_dll.led_init, apm3_dll.led_set_leds, led_port_no);

    if (FAILED(hr)) {
        goto fail;
    }

    hr = security_hook_init();

    if (FAILED(hr)) {
        goto fail;
    }

    touch_screen_hook_init(&apm3_hook_cfg.touch, apm3_hook_mod);

    mount_hook_init(&apm3_hook_cfg.platform.vfs, &apm3_hook_cfg.mount);

    security_hook_insert_hooks(NULL);

    mount_hook_apply_hooks(NULL);

    unity_hook_init(&apm3_hook_cfg.unity, apm3_hook_mod, unity_hook_callback);
    
    /* Initialize debug helpers */

    spike_hook_init(get_config_path());

    dprintf("---  End  %s ---\n", __func__);

    return apm3_startup();

fail:
    ExitProcess(EXIT_FAILURE);
}

BOOL WINAPI DllMain(HMODULE mod, DWORD cause, void *ctx)
{
    HRESULT hr;

    if (cause != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    wchar_t szFileName[MAX_PATH];
    GetModuleFileNameW(NULL, szFileName, MAX_PATH);
    if (wcsstr(szFileName, L"rundll32") != NULL) {
        return TRUE;
    }

    apm3_hook_mod = mod;
    hr = process_hijack_startup(apm3_pre_startup, &apm3_startup);

    if (!SUCCEEDED(hr)) {
        dprintf("Failed to hijack process startup: %x\n", (int) hr);
    }

    return SUCCEEDED(hr);
}
