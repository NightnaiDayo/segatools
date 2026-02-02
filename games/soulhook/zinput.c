#include <windows.h>
#include <dinput.h>
#include <xinput.h>

#include <shlwapi.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "board/io4.h"

#include "hook/table.h"
#include "util/dprintf.h"
#include "util/lib.h"

#include "swdchook/config.h"
#include "swdchook/zinput.h"

static struct zinput_config zinput_config;
static bool zinput_hook_initted;
static bool zinput_controller_init = false;

/* Hooked functions */
DWORD WINAPI hook_XInputGetState(DWORD dwUserIndex, XINPUT_STATE *pState);
DWORD WINAPI hook_XInputGetDSoundAudioDeviceGuids(DWORD dwUserIndex, GUID* pDSoundRenderGuid, GUID* pDSoundCaptureGuid);

HRESULT WINAPI hook_DirectInput8Create(
         HINSTANCE hinst,
         DWORD dwVersion,
         REFIID riidltf,
         LPVOID *ppvOut,
         LPUNKNOWN punkOuter);

static unsigned long WINAPI hook_AddRef(IUnknown *self);

static unsigned long WINAPI hook_Release(IUnknown *self);

static HRESULT WINAPI hook_CreateDevice(
        IDirectInput8W *self,
        REFGUID rguid,
        LPDIRECTINPUTDEVICE8W * lplpDirectInputDevice,
        LPUNKNOWN pUnkOuter);

static HRESULT WINAPI hook_EnumDevices(
        IDirectInput8W *self,
        DWORD dwDevType,
        LPDIENUMDEVICESCALLBACKW lpCallback,
        LPVOID pvRef,
        DWORD dwFlags);

static HRESULT WINAPI hook_SetDataFormat(
        IDirectInputDevice8W *self,
        LPCDIDATAFORMAT lpdf);

static HRESULT WINAPI hook_SetCooperativeLevel(
        IDirectInputDevice8W *self,
        HWND hwnd,
        DWORD flags);

static HRESULT WINAPI hook_Acquire(IDirectInputDevice8W *self);

static HRESULT WINAPI hook_Unacquire(IDirectInputDevice8W *self);

static HRESULT WINAPI hook_GetDeviceState(
        IDirectInputDevice8W *self,
        DWORD cbData,
        LPVOID lpvData);

static const IDirectInput8WVtbl api_vtbl = {
    .AddRef             = (void *) hook_AddRef,
    .Release            = (void *) hook_Release,
    .CreateDevice       = hook_CreateDevice,
    .EnumDevices        = hook_EnumDevices,
};

static const IDirectInput8W api = { (void *) &api_vtbl };

static const IDirectInputDevice8WVtbl dev_vtbl = {
    .AddRef             = (void *) hook_AddRef,
    .Release            = (void *) hook_Release,
    .SetDataFormat      = hook_SetDataFormat,
    .SetCooperativeLevel= hook_SetCooperativeLevel,
    .Acquire            = hook_Acquire,
    .Unacquire          = hook_Unacquire,
    .GetDeviceState     = hook_GetDeviceState,
};

static const IDirectInputDevice8W dev = { (void *) &dev_vtbl };

// Yup SEGA imports XInput functions via ordinal. FUN!
static struct hook_symbol zinput_hook_syms[] = {
    {
        .name       = "XInputGetState",
        .ordinal    = 0x0002,
        .patch      = hook_XInputGetState,
        .link       = NULL
    }, {
        .name       = "XInputGetDSoundAudioDeviceGuids",
        .ordinal    = 0x0005,
        .patch      = hook_XInputGetDSoundAudioDeviceGuids,
        .link       = NULL
    },
};

static const struct hook_symbol dinput_hook_syms[] = {
    {
        .name   = "DirectInput8Create",
        .patch  = hook_DirectInput8Create,
    }
};

void zinput_hook_init(struct zinput_config *cfg)
{
    wchar_t *module_path;
    wchar_t *file_name;

    assert(cfg != NULL);

    if (!cfg->enable) {
        return;
    }

    if (zinput_hook_initted) {
        return;
    }

    module_path = module_file_name(NULL);

    if (module_path != NULL) {
        file_name = PathFindFileNameW(module_path);

        free(module_path);
        module_path = NULL;

        _wcslwr(file_name);

        if (wcsstr(file_name, L"amdaemon") != NULL) {
            // dprintf("Executable filename contains 'amdaemon', disabling zinput\n");
            return;
        }
    }

    hook_table_apply(
        NULL,
        "XINPUT1_3.dll",
        zinput_hook_syms,
        _countof(zinput_hook_syms));

    hook_table_apply(
        NULL,
        "dinput8.dll",
        zinput_hook_syms,
        _countof(zinput_hook_syms));


    zinput_hook_initted = true;

    dprintf("ZInput: Hooking built-in XInput/DInput support\n");
}

DWORD WINAPI hook_XInputGetState(DWORD dwUserIndex, XINPUT_STATE *pState) {
    return ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI hook_XInputGetDSoundAudioDeviceGuids(DWORD dwUserIndex, GUID* pDSoundRenderGuid, GUID* pDSoundCaptureGuid) {
    return ERROR_DEVICE_NOT_CONNECTED;
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

    return S_OK;
}

static unsigned long WINAPI hook_AddRef(IUnknown *self)
{
    return 1;
}

static unsigned long WINAPI hook_Release(IUnknown *self)
{
    return 1;
}

static HRESULT WINAPI hook_CreateDevice(
        IDirectInput8W *self,
        REFGUID rguid,
        LPDIRECTINPUTDEVICE8W *lplpDirectInputDevice,
        LPUNKNOWN pUnkOuter)
{
    dprintf("ZInput: %s\n", __func__);
    *lplpDirectInputDevice = (void *) &dev;

    return S_OK;
}

static HRESULT WINAPI hook_EnumDevices(
        IDirectInput8W *self,
        DWORD dwDevType,
        LPDIENUMDEVICESCALLBACKW lpCallback,
        LPVOID pvRef,
        DWORD dwFlags)
{
    dprintf("ZInput: %s\n", __func__);

    return S_OK;
}

static HRESULT WINAPI hook_SetDataFormat(
        IDirectInputDevice8W *self,
        LPCDIDATAFORMAT lpdf)
{
    dprintf("ZInput: %s\n", __func__);

    return S_OK;
}

static HRESULT WINAPI hook_SetCooperativeLevel(
        IDirectInputDevice8W *self,
        HWND hwnd,
        DWORD flags)
{
    dprintf("ZInput: %s\n", __func__);

    return S_OK;
}

static HRESULT WINAPI hook_Acquire(IDirectInputDevice8W *self)
{
    return S_OK;
}

static HRESULT WINAPI hook_Unacquire(IDirectInputDevice8W *self)
{
    return S_OK;
}

static HRESULT WINAPI hook_GetDeviceState(
        IDirectInputDevice8W *self,
        DWORD cbData,
        LPVOID lpvData)
{
    memset(lpvData, 0, cbData);

    return S_OK;
}
