#include <windows.h>
#include <shlwapi.h>
#include <dinput.h>

#include <assert.h>
#include <stdlib.h>

#include "tokyohook/config.h"
#include "tokyohook/zinput.h"

#include "hook/table.h"

#include "util/lib.h"
#include "util/dprintf.h"

HRESULT WINAPI hook_DirectInput8Create(
         HINSTANCE hinst,
         DWORD dwVersion,
         REFIID riidltf,
         LPVOID *ppvOut,
         LPUNKNOWN punkOuter);

static HRESULT WINAPI hook_EnumDevices(
        IDirectInput8W *self,
        DWORD dwDevType,
        LPDIENUMDEVICESCALLBACKW lpCallback,
        LPVOID pvRef,
        DWORD dwFlags);

static unsigned long WINAPI hook_AddRef(IUnknown *self);
static unsigned long WINAPI hook_Release(IUnknown *self);

static const IDirectInput8WVtbl api_vtbl = {
    .EnumDevices        = hook_EnumDevices,
    .AddRef             = (void *) hook_AddRef,
    .Release            = (void *) hook_Release,
};

static const IDirectInput8W api = { (void *) &api_vtbl };

static const struct hook_symbol zinput_hook_syms[] = {
    {
        .name   = "DirectInput8Create",
        .patch  = hook_DirectInput8Create,
        .link   = NULL,
    }
};

HRESULT zinput_hook_init(struct zinput_config *cfg)
{
    wchar_t *module_path;
    wchar_t *file_name;

    assert(cfg != NULL);

    if (!cfg->enable) {
        return S_FALSE;
    }

    module_path = module_file_name(NULL);

    if (module_path != NULL) {
        file_name = PathFindFileNameW(module_path);

        free(module_path);
        module_path = NULL;

        _wcslwr(file_name);

        if (wcsstr(file_name, L"amdaemon") != NULL) {
            // dprintf("Executable filename contains 'amdaemon', disabling zinput\n");
            return S_OK;
        }
    }

    hook_table_apply(
            NULL,
            "dinput8.dll",
            zinput_hook_syms,
            _countof(zinput_hook_syms));

    return S_OK;
}

HRESULT WINAPI hook_DirectInput8Create(
         HINSTANCE hinst,
         DWORD dwVersion,
         REFIID riidltf,
         LPVOID *ppvOut,
         LPUNKNOWN punkOuter)
{
    dprintf("ZInput: Blocking built-in DirectInput support\n");
    *ppvOut = (void *) &api;

    return DI_OK;
}

static HRESULT WINAPI hook_EnumDevices(
        IDirectInput8W *self,
        DWORD dwDevType,
        LPDIENUMDEVICESCALLBACKW lpCallback,
        LPVOID pvRef,
        DWORD dwFlags)
{
    dprintf("ZInput: %s\n", __func__);

    return DI_OK;
}

static unsigned long WINAPI hook_AddRef(IUnknown *self)
{
    return 1;
}

static unsigned long WINAPI hook_Release(IUnknown *self)
{
    return 1;
}
