#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "board/io4.h"

#include "idachook/idac-dll.h"

#include "util/dprintf.h"

static HRESULT idac_io4_poll(void* ctx, struct io4_state* state);

static HRESULT idac_io4_write_gpio(uint8_t* payload, size_t len);

static uint16_t coins;

static const struct io4_ops idac_io4_ops = {
    .poll = idac_io4_poll,
    .write_gpio = idac_io4_write_gpio
};

static const uint16_t idac_gear_signals[] = {
    /* Neutral */
    0x0000,
    /* 1: Left|Up */
    0x0028,
    /* 2: Left|Down */
    0x0018,
    /* 3: Up */
    0x0020,
    /* 4: Down */
    0x0010,
    /* 5: Right|Up */
    0x0024,
    /* 6: Right|Down */
    0x0014,
};

HRESULT idac_io4_hook_init(const struct io4_config* cfg) {
    HRESULT hr;

    assert(idac_dll.init != NULL);

    hr = io4_hook_init(cfg, &idac_io4_ops, NULL, L"GameProject", false);

    if (FAILED(hr)) {
        return hr;
    }

    return idac_dll.init();
}

static HRESULT idac_io4_poll(void* ctx, struct io4_state* state) {
    uint8_t opbtn;
    uint8_t gamebtn;
    uint8_t gear;
    struct idac_io_analog_state analog_state;
    HRESULT hr;

    assert(idac_dll.get_opbtns != NULL);
    assert(idac_dll.get_gamebtns != NULL);
    assert(idac_dll.get_analogs != NULL);
    assert(idac_dll.get_shifter != NULL);

    memset(state, 0, sizeof(*state));
    memset(&analog_state, 0, sizeof(analog_state));
    opbtn = 0;
    gamebtn = 0;
    gear = 0;

    idac_dll.get_opbtns(&opbtn);
    idac_dll.get_gamebtns(&gamebtn);
    idac_dll.get_shifter(&gear);
    idac_dll.get_analogs(&analog_state);

    if (opbtn & IDAC_IO_OPBTN_TEST) {
        state->buttons[0] |= IO4_BUTTON_TEST;
    }

    if (opbtn & IDAC_IO_OPBTN_SERVICE) {
        state->buttons[0] |= IO4_BUTTON_SERVICE;
    }

    if (opbtn & IDAC_IO_OPBTN_COIN) {
        coins++;
    }
    state->chutes[0] = coins << 8;

    if (gamebtn & IDAC_IO_GAMEBTN_START) {
        state->buttons[0] |= 1 << 7;
    }

    if (gamebtn & IDAC_IO_GAMEBTN_VIEW_CHANGE) {
        state->buttons[0] |= 1 << 1;
    }

    if (gamebtn & IDAC_IO_GAMEBTN_UP) {
        state->buttons[0] |= 1 << 5;
    }

    if (gamebtn & IDAC_IO_GAMEBTN_DOWN) {
        state->buttons[0] |= 1 << 4;
    }

    if (gamebtn & IDAC_IO_GAMEBTN_LEFT) {
        state->buttons[0] |= 1 << 3;
    }

    if (gamebtn & IDAC_IO_GAMEBTN_RIGHT) {
        state->buttons[0] |= 1 << 2;
    }

    /* Update simulated six-speed shifter */

    if (gear > 6) {
        gear = 6;
    }

    state->buttons[1] = idac_gear_signals[gear];

    /* Steering wheel increases left-to-right.

       Use 0x8000 as the center point. */

    state->adcs[0] = 0x8000 + analog_state.wheel;
    state->adcs[1] = analog_state.accel;
    state->adcs[2] = analog_state.brake;

    return S_OK;
}

static HRESULT idac_io4_write_gpio(uint8_t* payload, size_t len) {
    assert(idac_dll.led_set_leds != NULL);

    // Just fast fail if there aren't enough bytes in the payload
    if (len < 3)
        return S_OK;

    // This command is used for lights in IDAC, but it only contains button lights,
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
        lights_data & IDAC_IO_LED_START ? 0xFF : 0x00,
        lights_data & IDAC_IO_LED_VIEW_CHANGE ? 0xFF : 0x00,
        lights_data & IDAC_IO_LED_UP ? 0xFF : 0x00,
        lights_data & IDAC_IO_LED_DOWN ? 0xFF : 0x00,
        lights_data & IDAC_IO_LED_RIGHT ? 0xFF : 0x00,
        lights_data & IDAC_IO_LED_LEFT ? 0xFF : 0x00,
    };

    idac_dll.led_set_leds(0, rgb_out);

    return S_OK;
}