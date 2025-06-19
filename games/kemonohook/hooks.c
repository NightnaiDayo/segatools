#include <stdlib.h>

#include "hook/iohook.h"
#include "hook/procaddr.h"
#include "hook/table.h"

#include "hooklib/serial.h"

#include "kemonohook/hooks.h"

#include "util/dprintf.h"

static BOOL WINAPI hook_GetVersionExW(LPOSVERSIONINFOW lpVersionInformation);

static int (WINAPI *next_GetVersionExW)(LPOSVERSIONINFOW lpVersionInformation);

static const struct hook_symbol kemono_kernel32_syms[] = {
        {
                .name = "GetVersionExW",
                .patch = hook_GetVersionExW,
                .link = (void **) &next_GetVersionExW,
        }
};

void kemono_extra_hooks_init(){
    HMODULE serialportapi = LoadLibraryA("Parade_Data/Plugins/SerialPortAPI.dll"); // HACK??
    if (serialportapi != NULL){
        iohook_apply_hooks(serialportapi);
        serial_hook_apply_hooks(serialportapi);
        dprintf("Kemono: Successfully pre-loaded SerialPortAPI\n");
    }
}

void kemono_extra_hooks_load(HMODULE mod, const wchar_t* target_module) {

    // Workaround for AmManager.checkTarget:Environment.GetEnvironmentVariable("USERNAME")
    SetEnvironmentVariableA("USERNAME", "AppUser");

    // Workaround for AmManager.checkTarget, expects OS version to be 6.2 or 6.3
    hook_table_apply(
            mod,
            "kernel32.dll",
            kemono_kernel32_syms,
            _countof(kemono_kernel32_syms));

    // needed for LED COM port
    // FIXME: SerialPortAPI.dll seems to be loaded twice? this causes a crash
    /*if (_wcsicmp(L"SerialPortAPI.dll", target_module) == 0) {
        iohook_apply_hooks(mod);
        serial_hook_apply_hooks(mod);
        dprintf("Kemono: Loaded I/O hooks for serial port\n");
    }*/

}

static BOOL WINAPI hook_GetVersionExW(LPOSVERSIONINFOW lpVersionInformation) {
    int result = next_GetVersionExW(lpVersionInformation);

    if (result) {
        lpVersionInformation->dwMajorVersion = 6;
        lpVersionInformation->dwMinorVersion = 2;
        lpVersionInformation->dwBuildNumber = 0;
        dprintf("Kemono: GetVersionExW hook hit\n");
    }

    return result;
}