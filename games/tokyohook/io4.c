#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "board/io4.h"

#include "tokyohook/tokyo-dll.h"

#include "util/dprintf.h"

static HRESULT tokyo_io4_poll(void *ctx, struct io4_state *state);
static HRESULT tokyo_io4_write_gpio(uint8_t* payload, size_t len);

static uint16_t coins;

static const struct io4_ops tokyo_io4_ops = {
    .poll       = tokyo_io4_poll,
    .write_gpio = tokyo_io4_write_gpio,
};

HRESULT tokyo_io4_hook_init(const struct io4_config *cfg)
{
    HRESULT hr;

    assert(tokyo_dll.init != NULL);

    hr = io4_hook_init(cfg, &tokyo_io4_ops, NULL);

    if (FAILED(hr)) {
        return hr;
    }

    return tokyo_dll.init();
}

static HRESULT tokyo_io4_poll(void *ctx, struct io4_state *state)
{
    uint8_t opbtn;
    uint8_t gamebtn;
    uint8_t sense;
    HRESULT hr;

    assert(tokyo_dll.get_opbtns != NULL);
    assert(tokyo_dll.get_gamebtns != NULL);
    assert(tokyo_dll.get_sensors != NULL);

    memset(state, 0, sizeof(*state));

    opbtn = 0;
    gamebtn = 0;
    sense = 0;

    tokyo_dll.get_opbtns(&opbtn);
    tokyo_dll.get_gamebtns(&gamebtn);
    tokyo_dll.get_sensors(&sense);

    if (opbtn & TOKYO_IO_OPBTN_TEST) {
        state->buttons[0] |= IO4_BUTTON_TEST;
    }

    if (opbtn & TOKYO_IO_OPBTN_SERVICE) {
        state->buttons[0] |= IO4_BUTTON_SERVICE;
    }

    if (opbtn & TOKYO_IO_OPBTN_COIN) {
        coins++;
    }
    state->chutes[0] = coins << 8;

    /* Update gamebtns */

    if (gamebtn & TOKYO_IO_GAMEBTN_BLUE) {
        state->buttons[0] |= 1 << 1;
    }

    if (gamebtn & TOKYO_IO_GAMEBTN_YELLOW) {
        state->buttons[0] |= 1 << 0;
    }

    if (gamebtn & TOKYO_IO_GAMEBTN_RED) {
        state->buttons[0] |= 1 << 15;
    }

    /* Update sensors */

    // Invert the logic so that it's active high
    if (!(sense & TOKYO_IO_SENSE_FOOT_LEFT)) {
        state->buttons[0] |= 1 << 13;
    }

    if (!(sense & TOKYO_IO_SENSE_FOOT_RIGHT)) {
        state->buttons[1] |= 1 << 13;
    }

    if (sense & TOKYO_IO_SENSE_JUMP_1) {
        state->buttons[0] |= 1 << 12;
    }

    if (sense & TOKYO_IO_SENSE_JUMP_2) {
        state->buttons[1] |= 1 << 12;
    }

    if (sense & TOKYO_IO_SENSE_JUMP_3) {
        state->buttons[0] |= 1 << 11;
    }

    if (sense & TOKYO_IO_SENSE_JUMP_4) {
        state->buttons[1] |= 1 << 11;
    }

    if (sense & TOKYO_IO_SENSE_JUMP_5) {
        state->buttons[0] |= 1 << 10;
    }

    if (sense & TOKYO_IO_SENSE_JUMP_6) {
        state->buttons[1] |= 1 << 10;
    }

    return S_OK;
}

static HRESULT tokyo_io4_write_gpio(uint8_t* payload, size_t len)
{
    // Just fast fail if there aren't enough bytes in the payload
    if (len < 3) 
        return S_OK;

    // This command is used for lights in Mario & Sonic at the Tokyo 2020 Olympics
    // Arcade, but it only contains button lights, and only in the first 3 bytes of
    // the payload; everything else is padding to make the payload 62 bytes. The 
    // rest of the cabinet lights and the side button lights are handled separately,
    // by the 15093 lights controller.
    uint32_t lights_data = (uint32_t) ((uint8_t)(payload[0]) << 24 |
        (uint8_t)(payload[1]) << 16 |
        (uint8_t)(payload[2]) << 8);

    // Since Sega uses an odd ordering for the first part of the bitfield,
    // let's normalize the data and just send over bytes for the receiver
    // to interpret as RGB values.
    uint8_t rgb_out[5 * 3] = {
        lights_data & TOKYO_IO_LED_LEFT_BLUE ? 0xFF : 0x00,
        lights_data & TOKYO_IO_LED_CENTER_YELLOW ? 0xFF : 0x00,
        lights_data & TOKYO_IO_LED_RIGHT_RED ? 0xFF : 0x00,
        lights_data & TOKYO_IO_LED_CONTROL_LEFT_R ? 0xFF : 0x00,
        lights_data & TOKYO_IO_LED_CONTROL_LEFT_G ? 0xFF : 0x00,
        lights_data & TOKYO_IO_LED_CONTROL_LEFT_B ? 0xFF : 0x00,
        lights_data & TOKYO_IO_LED_CONTROL_RIGHT_R ? 0xFF : 0x00,
        lights_data & TOKYO_IO_LED_CONTROL_RIGHT_G ? 0xFF : 0x00,
        lights_data & TOKYO_IO_LED_CONTROL_RIGHT_B ? 0xFF : 0x00,
        lights_data & TOKYO_IO_LED_FLOOR_LEFT_R ? 0xFF : 0x00,
        lights_data & TOKYO_IO_LED_FLOOR_LEFT_G ? 0xFF : 0x00,
        lights_data & TOKYO_IO_LED_FLOOR_LEFT_B ? 0xFF : 0x00,
        lights_data & TOKYO_IO_LED_FLOOR_RIGHT_R ? 0xFF : 0x00,
        lights_data & TOKYO_IO_LED_FLOOR_RIGHT_G ? 0xFF : 0x00,
        lights_data & TOKYO_IO_LED_FLOOR_RIGHT_B ? 0xFF : 0x00,
    };

    tokyo_dll.led_set_leds(1, rgb_out);

    return S_OK;
}
