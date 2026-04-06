#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "board/io4.h"

#include "util/dprintf.h"

#include "swdchook/swdc-dll.h"

static HANDLE mmf;

static HRESULT init_mmf(void);

static void swdc_set_gamebtns(uint16_t value);

static HRESULT swdc_io4_poll(void* ctx, struct io4_state* state);

static HRESULT swdc_io4_write_gpio(uint8_t* payload, size_t len);

static uint16_t coins;

static const struct io4_ops swdc_io4_ops = {
    .poll = swdc_io4_poll,
    .write_gpio = swdc_io4_write_gpio
};

HRESULT swdc_io4_hook_init(const struct io4_config* cfg) {
    HRESULT hr;

    assert(swdc_dll.init != NULL);

    hr = io4_hook_init(cfg, &swdc_io4_ops, NULL, L"Todoroki", true);

    if (FAILED(hr)) {
        return hr;
    }

    hr = init_mmf();

    if (FAILED(hr)) {
        return hr;
    }

    return swdc_dll.init();
}

// Function to initialize the memory-mapped file
static HRESULT init_mmf(void) {
    mmf = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 2, "SWDCButton");
    if (mmf == NULL) {
        return S_FALSE;
    }

    swdc_set_gamebtns(0);

    return S_OK;
}

void swdc_set_gamebtns(uint16_t value) {
    // WaitForSingleObject(mutex, INFINITE);

    // Update the memory-mapped file
    LPVOID mmf_view = MapViewOfFile(mmf, FILE_MAP_ALL_ACCESS, 0, 0, 2);
    if (mmf_view != NULL) {
        uint16_t* ptr = (uint16_t *) mmf_view;
        *ptr = value;

        UnmapViewOfFile(mmf_view);
    }

    // ReleaseMutex(mutex);
}

static HRESULT swdc_io4_poll(void* ctx, struct io4_state* state) {
    uint8_t opbtn;
    uint16_t gamebtn;
    struct swdc_io_analog_state analog_state;
    HRESULT hr;

    assert(swdc_dll.get_opbtns != NULL);
    assert(swdc_dll.get_gamebtns != NULL);
    assert(swdc_dll.get_analogs != NULL);

    memset(state, 0, sizeof(*state));
    memset(&analog_state, 0, sizeof(analog_state));
    opbtn = 0;
    gamebtn = 0;

    swdc_dll.get_opbtns(&opbtn);
    swdc_dll.get_gamebtns(&gamebtn);
    swdc_dll.get_analogs(&analog_state);

    if (opbtn & SWDC_IO_OPBTN_TEST) {
        state->buttons[0] |= IO4_BUTTON_TEST;
    }

    if (opbtn & SWDC_IO_OPBTN_SERVICE) {
        state->buttons[0] |= IO4_BUTTON_SERVICE;
    }

    if (opbtn & SWDC_IO_OPBTN_COIN) {
        coins++;
    }
    state->chutes[0] = coins << 8;

    /* Update Cabinet buttons */

    if (gamebtn & SWDC_IO_GAMEBTN_START) {
        state->buttons[0] |= 1 << 7;
    }

    if (gamebtn & SWDC_IO_GAMEBTN_VIEW_CHANGE) {
        state->buttons[0] |= 1 << 1;
    }

    if (gamebtn & SWDC_IO_GAMEBTN_UP) {
        state->buttons[0] |= 1 << 5;
    }

    if (gamebtn & SWDC_IO_GAMEBTN_DOWN) {
        state->buttons[0] |= 1 << 4;
    }

    if (gamebtn & SWDC_IO_GAMEBTN_LEFT) {
        state->buttons[0] |= 1 << 3;
    }

    if (gamebtn & SWDC_IO_GAMEBTN_RIGHT) {
        state->buttons[0] |= 1 << 2;
    }

    /* 
    Update steering wheel buttons

    Those are connected to the SEGA 838-15415 INDICATOR BD MAIN 
    USB board which is not emulated for now. So those buttons
    are hooked to the built-in XInput support.
*/

    /* Instead update gamebtns for the file mapping */

    swdc_set_gamebtns(gamebtn);

    if (gamebtn & SWDC_IO_GAMEBTN_STEERING_BLUE) {
        state->buttons[1] |= 1 << 15;
    }

    if (gamebtn & SWDC_IO_GAMEBTN_STEERING_GREEN) {
        state->buttons[1] |= 1 << 14;
    }

    if (gamebtn & SWDC_IO_GAMEBTN_STEERING_RED) {
        state->buttons[1] |= 1 << 13;
    }

    if (gamebtn & SWDC_IO_GAMEBTN_STEERING_YELLOW) {
        state->buttons[1] |= 1 << 12;
    }

    if (gamebtn & SWDC_IO_GAMEBTN_STEERING_PADDLE_LEFT) {
        state->buttons[1] |= 1 << 1;
    }

    if (gamebtn & SWDC_IO_GAMEBTN_STEERING_PADDLE_RIGHT) {
        state->buttons[1] |= 1 << 0;
    }

    /* Steering wheel increases left-to-right.

       Use 0x8000 as the center point. */

    state->adcs[0] = 0x8000 + analog_state.wheel;
    state->adcs[1] = analog_state.accel;
    state->adcs[2] = analog_state.brake;

    return S_OK;
}

static HRESULT swdc_io4_write_gpio(uint8_t* payload, size_t len) {
    assert(swdc_dll.led_set_leds != NULL);

    // Just fast fail if there aren't enough bytes in the payload
    if (len < 3)
        return S_OK;

    // This command is used for lights in SWDC, but it only contains button lights,
    // and only in the first 3 bytes of the payload; everything else is padding to
    // make the payload 62 bytes. The rest of the cabinet lights and the side button
    // lights are handled separately, by the 15070 lights controller.
    uint32_t lights_data = (uint32_t)((uint8_t)(payload[0]) << 24 |
                                      (uint8_t)(payload[1]) << 16 |
                                      (uint8_t)(payload[2]) << 8);

    // Since Sega uses an odd ordering for the first part of the bitfield,
    // let's normalize the data and just send over bytes for the receiver
    // to interpret as ON/OFF values.
    uint8_t rgb_out[6] = {
        lights_data & SWDC_IO_LED_START ? 0xFF : 0x00,
        lights_data & SWDC_IO_LED_VIEW_CHANGE ? 0xFF : 0x00,
        lights_data & SWDC_IO_LED_UP ? 0xFF : 0x00,
        lights_data & SWDC_IO_LED_DOWN ? 0xFF : 0x00,
        lights_data & SWDC_IO_LED_RIGHT ? 0xFF : 0x00,
        lights_data & SWDC_IO_LED_LEFT ? 0xFF : 0x00,
    };

    swdc_dll.led_set_leds(0, rgb_out);

    return S_OK;
}