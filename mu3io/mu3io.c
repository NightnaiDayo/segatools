#include <windows.h>
#include <xinput.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "mu3io/mu3io.h"
#include "mu3io/config.h"
#include "mu3io/ledoutput.h"

#include "util/dprintf.h"
#include "util/env.h"

static uint8_t mu3_opbtn;
static uint8_t mu3_left_btn;
static uint8_t mu3_right_btn;
static int16_t mu3_lever_pos;
static int16_t mu3_lever_xpos;
static struct mu3_io_config mu3_io_cfg;
static bool mu3_io_coin;

// Mouse control factor to adjust the speed of mouse movement
const double MOUSE_SENSITIVITY = 0.5;

uint16_t mu3_io_get_api_version(void)
{
    return 0x0101;
}

HRESULT mu3_io_init(void)
{
    mu3_io_config_load(&mu3_io_cfg, get_config_path());

    dprintf("XInput: --- Begin configuration ---\n");
    dprintf("XInput: Mouse lever emulation : %i\n", mu3_io_cfg.use_mouse);
    dprintf("XInput: ---  End  configuration ---\n");

    mu3_led_init_mutex = CreateMutex(
        NULL,              // default security attributes
        FALSE,             // initially not owned
        NULL);             // unnamed mutex
    
    if (mu3_led_init_mutex == NULL)
    {
        return E_FAIL;
    }

    return mu3_led_output_init(&mu3_io_cfg);
}

HRESULT mu3_io_poll(void)
{
    int lever;
    int xlever;
    XINPUT_STATE xi;
    WORD xb;

    mu3_opbtn = 0;
    mu3_left_btn = 0;
    mu3_right_btn = 0;

    if (GetAsyncKeyState(mu3_io_cfg.vk_test) & 0x8000) {
        mu3_opbtn |= MU3_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(mu3_io_cfg.vk_service) & 0x8000) {
        mu3_opbtn |= MU3_IO_OPBTN_SERVICE;
    }

    if (GetAsyncKeyState(mu3_io_cfg.vk_coin) & 0x8000) {
        if (!mu3_io_coin) {
            mu3_io_coin = true;
            mu3_opbtn |= MU3_IO_OPBTN_COIN;
        }
    } else {
        mu3_io_coin = false;
    }

    memset(&xi, 0, sizeof(xi));
    XInputGetState(0, &xi);
    xb = xi.Gamepad.wButtons;

    if (GetAsyncKeyState(mu3_io_cfg.vk_left_1) || (xb & XINPUT_GAMEPAD_DPAD_LEFT)) {
        mu3_left_btn |= MU3_IO_GAMEBTN_1;
    }

    if (GetAsyncKeyState(mu3_io_cfg.vk_left_2) || (xb & XINPUT_GAMEPAD_DPAD_UP)) {
        mu3_left_btn |= MU3_IO_GAMEBTN_2;
    }

    if (GetAsyncKeyState(mu3_io_cfg.vk_left_3) || (xb & XINPUT_GAMEPAD_DPAD_RIGHT)) {
        mu3_left_btn |= MU3_IO_GAMEBTN_3;
    }

    if (GetAsyncKeyState(mu3_io_cfg.vk_right_1) || (xb & XINPUT_GAMEPAD_X)) {
        mu3_right_btn |= MU3_IO_GAMEBTN_1;
    }

    if (GetAsyncKeyState(mu3_io_cfg.vk_right_2) || (xb & XINPUT_GAMEPAD_Y)) {
        mu3_right_btn |= MU3_IO_GAMEBTN_2;
    }

    if (GetAsyncKeyState(mu3_io_cfg.vk_right_3) || (xb & XINPUT_GAMEPAD_B)) {
        mu3_right_btn |= MU3_IO_GAMEBTN_3;
    }

    if (GetAsyncKeyState(mu3_io_cfg.vk_left_menu) || (xb & XINPUT_GAMEPAD_BACK)) {
        mu3_left_btn |= MU3_IO_GAMEBTN_MENU;
    }

    if (GetAsyncKeyState(mu3_io_cfg.vk_right_menu) || (xb & XINPUT_GAMEPAD_START)) {
        mu3_right_btn |= MU3_IO_GAMEBTN_MENU;
    }

    if (GetAsyncKeyState(mu3_io_cfg.vk_left_side) || (xb & XINPUT_GAMEPAD_LEFT_SHOULDER)) {
        mu3_left_btn |= MU3_IO_GAMEBTN_SIDE;
    }

    if (GetAsyncKeyState(mu3_io_cfg.vk_right_side) || (xb & XINPUT_GAMEPAD_RIGHT_SHOULDER)) {
        mu3_right_btn |= MU3_IO_GAMEBTN_SIDE;
    }

    lever = mu3_lever_pos;

    if (mu3_io_cfg.use_mouse) {
        // mouse movement
        POINT mousePos;
        GetCursorPos(&mousePos);

        // int mouseMovement = (int)(xi.Gamepad.sThumbLX * MOUSE_SENSITIVITY);
        // int newXPos = mousePos.x + mouseMovement;
        int mouse_x = mousePos.x;

        // clamp the mouse_x position to the screen width
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        if (mouse_x < 0) {
            mouse_x = 0;
        }
        else if (mouse_x > screenWidth) {
            mouse_x = screenWidth;
        }

        // normalize the mouse_x position from 0 to 1
        double mouse_x_norm = (double)mouse_x / screenWidth;

        // scale the mouse_x_norm to the range of INT16_MIN to INT16_MAX
        mouse_x = (int)((mouse_x_norm * (INT16_MAX - INT16_MIN)) + INT16_MIN);

        lever = mouse_x;
    } else {
        if (abs(xi.Gamepad.sThumbLX) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
            lever += xi.Gamepad.sThumbLX / 24;
        }

        if (abs(xi.Gamepad.sThumbRX) > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
            lever += xi.Gamepad.sThumbRX / 24;
        }
    }

    if (lever < INT16_MIN) {
        lever = INT16_MIN;
    }

    if (lever > INT16_MAX) {
        lever = INT16_MAX;
    }

    mu3_lever_pos = lever;

    xlever = mu3_lever_pos
                    - xi.Gamepad.bLeftTrigger * 64
                    + xi.Gamepad.bRightTrigger * 64;

    if (xlever < INT16_MIN) {
        xlever = INT16_MIN;
    }

    if (xlever > INT16_MAX) {
        xlever = INT16_MAX;
    }

    mu3_lever_xpos = xlever;

    return S_OK;
}

void mu3_io_get_opbtns(uint8_t *opbtn)
{
    if (opbtn != NULL) {
        *opbtn = mu3_opbtn;
    }
}

void mu3_io_get_gamebtns(uint8_t *left, uint8_t *right)
{
    if (left != NULL) {
        *left = mu3_left_btn;
    }

    if (right != NULL ){
        *right = mu3_right_btn;
    }
}

void mu3_io_get_lever(int16_t *pos)
{
    if (pos != NULL) {
        *pos = mu3_lever_xpos;
    }
}

HRESULT mu3_io_led_init(void)
{
    return S_OK;
}

void mu3_io_led_set_colors(uint8_t board, uint8_t *rgb)
{
    mu3_led_output_update(board, rgb);
}
