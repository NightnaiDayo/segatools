#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>

#include "platform/clock.h"

#include "hook/table.h"
#include "hook/procaddr.h"
#include "hook/iohook.h"

#include "hooklib/dll.h"
#include "hooklib/path.h"
#include "hooklib/printer_chc.h"
#include "hooklib/reg.h"
#include "hooklib/touch.h"
#include "hooklib/serial.h"

#include "util/dprintf.h"

#include "doorstop.h"
#include "hook.h"

static bool unity_hook_initted;
static struct unity_config unity_config;

static const wchar_t *target_modules[] = {
    L"mono.dll",
    L"mono-2.0-bdwgc.dll",
    L"cri_ware_unity.dll",
    L"amdaemon_api.dll",
    L"SerialPortAPI.dll",
    L"C300usb.dll",
    L"C300FWDLusb.dll",
    L"apmled.dll",
    L"HKBSys_api.dll",
    L"amptw.dll",
    L"apmmount.dll",
    L"abaasgs.dll",
    L"AVProVideo.dll",
    L"Audio360.dll",
};

static const size_t target_modules_len = _countof(target_modules);

static void dll_hook_insert_hooks(HMODULE target);

static unity_hook_callback_func hook_load_callback;

static HMODULE WINAPI hook_LoadLibraryW(const wchar_t *name);
static HMODULE (WINAPI *next_LoadLibraryW)(const wchar_t *name);
static HMODULE WINAPI hook_LoadLibraryExW(const wchar_t *name, HANDLE  hFile, DWORD   dwFlags);
static HMODULE (WINAPI *next_LoadLibraryExW)(const wchar_t *name, HANDLE  hFile, DWORD   dwFlags);

static const struct hook_symbol unity_kernel32_syms[] = {
    {
        .name = "LoadLibraryW",
        .patch = hook_LoadLibraryW,
        .link = (void **) &next_LoadLibraryW,
    }, {
        .name  = "LoadLibraryExW",
        .patch = hook_LoadLibraryExW,
        .link  = (void **) &next_LoadLibraryExW,
    }
};


void unity_hook_init(const struct unity_config *cfg, HINSTANCE self, unity_hook_callback_func callback) {
    assert(cfg != NULL);

    if (!cfg->enable) {
        return;
    }

    if (unity_hook_initted) {
        return;
    }

    memcpy(&unity_config, cfg, sizeof(*cfg));
    dll_hook_insert_hooks(NULL);

    hook_load_callback = callback;

    unity_hook_initted = true;
    dprintf("Unity: Hook enabled.\n");
}

static void dll_hook_insert_hooks(HMODULE target) {
    hook_table_apply(
        target,
        "kernel32.dll",
        unity_kernel32_syms,
        _countof(unity_kernel32_syms));
}

static HMODULE WINAPI hook_LoadLibraryExW(const wchar_t *name, HANDLE  hFile, DWORD   dwFlags)
{
    return hook_LoadLibraryW(name);
}

static HMODULE WINAPI hook_LoadLibraryW(const wchar_t *name)
{
    const wchar_t *name_end;
    const wchar_t *target_module;
    bool already_loaded;
    HMODULE result;
    size_t name_len;
    size_t target_module_len;

    if (name == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);

        return NULL;
    }

    // Check if the module is already loaded
    already_loaded = GetModuleHandleW(name) != NULL;

    // Must call the next handler so the DLL reference count is incremented
    result = next_LoadLibraryW(name);

    if (!already_loaded && result != NULL) {
        name_len = wcslen(name);

        // mono entrypoint for injecting target_assembly
        if (GetProcAddress(result, "mono_jit_init_version")) {
            doorstop_mono_hook_init(&unity_config, result);
        }

        for (size_t i = 0; i < target_modules_len; i++) {
            target_module = target_modules[i];
            target_module_len = wcslen(target_module);

            // Check if the newly loaded library is at least the length of
            // the name of the target module
            if (name_len < target_module_len) {
                continue;
            }

            name_end = &name[name_len - target_module_len];

            // Check if the name of the newly loaded library is one of the
            // modules the path hooks should be injected into
            if (_wcsicmp(name_end, target_module) != 0) {
                continue;
            }

            dprintf("Unity: Loaded %S\n", target_module);

            dll_hook_insert_hooks(result);
            path_hook_insert_hooks(result);
            iohook_apply_hooks(result);

            reg_hook_insert_hooks(result);
            clock_hook_insert_hooks(result);
            proc_addr_insert_hooks(result);
            if (hook_load_callback != NULL){
                hook_load_callback(result, target_module);
            }
        }
    }

    return result;
}
