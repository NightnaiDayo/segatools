#include <windows.h>
#include <shlwapi.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <xinput.h>

#include "board/io4.h"

#include "hook/table.h"
#include "util/dprintf.h"
#include "util/lib.h"

#include "swdchook/config.h"
#include "swdchook/zinput.h"

static struct zinput_config zinput_config;
static bool zinput_hook_initted;
static bool zinput_controller_init = false;

static HRESULT init_mmf(void);

static HANDLE mmf;
static uint16_t* swdc_gamebtn;

/* Hooked functions */
DWORD WINAPI hook_XInputGetState(DWORD dwUserIndex, XINPUT_STATE *pState);
DWORD WINAPI hook_XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);
// Not needed for now?
DWORD WINAPI hook_XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES *pCapabilities);

// Yup SEGA imports XInput functions via ordinal. FUN!
static struct hook_symbol zinput_hook_syms[] = {
    {
        .name       = "XInputGetState",
        .ordinal    = 0x0002,
        .patch      = hook_XInputGetState,
        .link       = NULL
    }, {
        .name       = "XInputSetState",
        .ordinal    = 0x0003,
        .patch      = hook_XInputSetState,
        .link       = NULL
    }, {
        // Not needed for now?
        .name       = "XInputGetCapabilities",
        .patch      = hook_XInputGetCapabilities,
        .link       = NULL
    },
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
    
    if (FAILED(init_mmf())) {
        return;
    }

    zinput_hook_initted = true;

    dprintf("ZInput: Hooking built-in XInput support\n");
}

bool zinput_connect_controller(bool enable) {
    zinput_controller_init = enable;
    dprintf("zinput_connect_controller\n");
    return true;
}

static HRESULT init_mmf(void) {
    // Create or open memory-mapped file
    mmf = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 2, "SWDCButton");
    if (mmf == NULL) {
        return S_FALSE;
    }
    
    // Map the memory-mapped file
    swdc_gamebtn = (uint16_t*)MapViewOfFile(mmf, FILE_MAP_ALL_ACCESS, 0, 0, 2);

    return S_OK;
}

DWORD WINAPI hook_XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES *pCapabilities) {
    // dprintf("ZInput: XInputGetCapabilities hook hit\n");

    if (!zinput_controller_init) {
        zinput_connect_controller(true);
    }

    if (dwFlags > XINPUT_FLAG_GAMEPAD) {
        return ERROR_BAD_ARGUMENTS;
    }

    if (zinput_controller_init && dwUserIndex == 0) {
        pCapabilities->Flags = XINPUT_CAPS_VOICE_SUPPORTED;
        pCapabilities->Type = XINPUT_DEVTYPE_GAMEPAD;
        pCapabilities->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;

        pCapabilities->Gamepad.wButtons = 0xF3FF;

        pCapabilities->Gamepad.bLeftTrigger = 0xFF;
        pCapabilities->Gamepad.bRightTrigger = 0xFF;

        pCapabilities->Gamepad.sThumbLX = (SHORT)0xFFC0;
        pCapabilities->Gamepad.sThumbLY = (SHORT)0xFFC0;
        pCapabilities->Gamepad.sThumbRX = (SHORT)0xFFC0;
        pCapabilities->Gamepad.sThumbRY = (SHORT)0xFFC0;

        pCapabilities->Vibration.wLeftMotorSpeed = 0xFF;
        pCapabilities->Vibration.wRightMotorSpeed = 0xFF;

        return ERROR_SUCCESS;
    } else {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
}

DWORD WINAPI hook_XInputGetState(DWORD dwUserIndex, XINPUT_STATE *pState) {
    // dprintf("ZInput: XInputGetState hook hit\n");

    if (!zinput_controller_init) {
        zinput_connect_controller(true);
    }

    if (zinput_controller_init && dwUserIndex == 0) {
        XINPUT_GAMEPAD gamepad_state = {0};
        gamepad_state.wButtons = 0;

        /* Read filemapping for for the gamebtns */

        if (*swdc_gamebtn & SWDC_IO_GAMEBTN_STEERING_PADDLE_LEFT) {
            gamepad_state.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
        }

        if (*swdc_gamebtn & SWDC_IO_GAMEBTN_STEERING_PADDLE_RIGHT) {
            gamepad_state.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
        }

        if (*swdc_gamebtn & SWDC_IO_GAMEBTN_STEERING_BLUE) {
            gamepad_state.wButtons |= XINPUT_GAMEPAD_X;
        }

        if (*swdc_gamebtn & SWDC_IO_GAMEBTN_STEERING_RED) {
            gamepad_state.wButtons |= XINPUT_GAMEPAD_B;
        }

        if (*swdc_gamebtn & SWDC_IO_GAMEBTN_STEERING_GREEN) {
            gamepad_state.wButtons |= XINPUT_GAMEPAD_A;
        }

        if (*swdc_gamebtn & SWDC_IO_GAMEBTN_STEERING_YELLOW) {
            gamepad_state.wButtons |= XINPUT_GAMEPAD_Y;
        }
        if (pState->dwPacketNumber == UINT_MAX)
            pState->dwPacketNumber = 0;
        else
            pState->dwPacketNumber++;

        pState->Gamepad = gamepad_state;
        return ERROR_SUCCESS;
    } else {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
}

DWORD WINAPI hook_XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration) {
    // dprintf("ZInput: XInputSetState hook hit\n");

    if (!zinput_controller_init) {
        zinput_connect_controller(true);
    }

    if (zinput_controller_init && dwUserIndex == 0) {
        return ERROR_SUCCESS;
    } else {
        return ERROR_DEVICE_NOT_CONNECTED;
    }
}
