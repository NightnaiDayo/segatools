#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "mercuryhook/elizabeth.h"
#include "mercuryhook/mercury-dll.h"

#include "hook/table.h"

#include "hooklib/uart.h"
#include "hooklib/dll.h"
#include "hooklib/path.h"
#include "hooklib/setupapi.h"

#include "util/dprintf.h"

/* Hooks targeted DLLs dynamically loaded by elizabeth. */

static void dll_hook_insert_hooks(HMODULE target);

/* Hook functions */

static FARPROC WINAPI hook_GetProcAddress(HMODULE hModule, const char *name);

static int hook_USBIntLED_Init();

static int hook_USBIntLED_set(int data1, struct led_data data2);

/* Link pointers */

static FARPROC (WINAPI *next_GetProcAddress)(HMODULE hModule, const char *name);


static const struct hook_symbol win32_hooks[] = {
    {
        .name = "GetProcAddress",
        .patch = hook_GetProcAddress,
        .link = (void **) &next_GetProcAddress
    }
};

HRESULT elizabeth_hook_init(struct elizabeth_config *cfg)
{
    if (!cfg->enable) {
        return S_OK;
    }

    dll_hook_insert_hooks(NULL);
    dprintf("Elizabeth: Init\n");

    return S_OK;
}

static void dll_hook_insert_hooks(HMODULE target)
{
    hook_table_apply(
            target,
            "kernel32.dll",
            win32_hooks,
            _countof(win32_hooks));
}

FARPROC WINAPI hook_GetProcAddress(HMODULE hModule, const char *name)
{
    uintptr_t ordinal = (uintptr_t) name;

    FARPROC result = next_GetProcAddress(hModule, name);

    if (ordinal > 0xFFFF) {
        /* Import by name */
        if (strcmp(name, "USBIntLED_Init") == 0) {
            result = (FARPROC) hook_USBIntLED_Init;
        }

        if (strcmp(name, "USBIntLED_set") == 0) {
            result = (FARPROC) hook_USBIntLED_set;
        }
    }

    return result;
}

/* Intercept the call to initialize the LED board. */
static int hook_USBIntLED_Init()
{
    dprintf("Elizabeth: hook_USBIntLED_Init hit!\n");
    return 1;
}

static int hook_USBIntLED_set(int data1, struct led_data data2)
{
    assert(mercury_dll.set_leds != NULL);
    mercury_dll.set_leds(data2);
    return 1;
}
