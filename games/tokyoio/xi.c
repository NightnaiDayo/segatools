#include <windows.h>
#include <xinput.h>
#include <math.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "tokyoio/backend.h"
#include "tokyoio/config.h"
#include "tokyoio/tokyoio.h"
#include "tokyoio/xi.h"

#include "util/dprintf.h"
#include "util/str.h"

static void tokyo_xi_get_gamebtns(uint8_t *gamebtn_out);
static void tokyo_xi_get_sensors(uint8_t *sense_out);

static const struct tokyo_io_backend tokyo_xi_backend = {
    .get_gamebtns   = tokyo_xi_get_gamebtns,
    .get_sensors    = tokyo_xi_get_sensors,
};

HRESULT tokyo_xi_init(const struct tokyo_io_backend **backend)
{
    wchar_t dll_path[MAX_PATH];
    HMODULE xinput;
    HRESULT hr;
    UINT path_pos;

    assert(backend != NULL);

    dprintf("TokyoIO: IO4: Using XInput controller\n");
    *backend = &tokyo_xi_backend;
    return S_OK;
}

static void tokyo_xi_get_gamebtns(uint8_t *gamebtn_out)
{
    uint8_t gamebtn;

    assert(gamebtn_out != NULL);

    gamebtn = 0;

    XINPUT_STATE xi;
    WORD xb;

    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);
    xb = xi.Gamepad.wButtons;

    /* PUSH BUTTON inputs */

    if ((xb & XINPUT_GAMEPAD_X) || (xb & XINPUT_GAMEPAD_DPAD_LEFT)) {
        gamebtn |= TOKYO_IO_GAMEBTN_BLUE;
    }

    if (xb & XINPUT_GAMEPAD_Y || (xb & XINPUT_GAMEPAD_A)) {
        gamebtn |= TOKYO_IO_GAMEBTN_YELLOW;
    }

    if ((xb & XINPUT_GAMEPAD_B) || (xb & XINPUT_GAMEPAD_DPAD_RIGHT)) {
        gamebtn |= TOKYO_IO_GAMEBTN_RED;
    }

    *gamebtn_out = gamebtn;
}

static void tokyo_xi_get_sensors(uint8_t *sense_out)
{
    uint8_t sense;

    XINPUT_STATE xi;
    WORD xb;
    BYTE xt_l;
    BYTE xt_r;

    assert(sense_out != NULL);

    sense = 0;

    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);
    xb = xi.Gamepad.wButtons;
    xt_l = xi.Gamepad.bLeftTrigger;
    xt_r = xi.Gamepad.bRightTrigger;

    float xt_l_f = xt_l / 255.0f;
    float xt_r_f = xt_r / 255.0f;

    // Normalize both triggers to 0..1 and find the max directly
    float trigger = fmaxf(xt_l_f, xt_r_f);

    const int max_jump_levels = 6;
    float jump_threshold = 1.0f / max_jump_levels;
    
    /* FOOT SENSOR inputs */

     // Determine if both foot sensors should be set
    bool left_active = xt_l_f > jump_threshold;
    bool right_active = xt_r_f > jump_threshold;

    // Set foot sensors based on individual trigger activity
    if (left_active) {
        sense |= TOKYO_IO_SENSE_FOOT_LEFT;
    }
    if (right_active) {
        sense |= TOKYO_IO_SENSE_FOOT_RIGHT;
    }

    /* JUMP SENSOR inputs */

    // If both triggers are active, set jump levels and both foot sensors
    if (left_active && right_active) {
        float trigger_avg = (xt_l_f + xt_r_f) / 2.0f;

        // Calculate the appropriate jump level
        for (int i = 1; i <= max_jump_levels; ++i) {
            if (trigger_avg >= i * jump_threshold) {
                sense |= (TOKYO_IO_SENSE_JUMP_1 << (i - 1));
            } else {
                break;
            }
        }
    }

    *sense_out = sense;
}
