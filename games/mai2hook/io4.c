#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "board/io4.h"

#include "mai2hook/mai2-dll.h"

#include "util/dprintf.h"

static HRESULT mai2_io4_poll(void* ctx, struct io4_state* state);
static HRESULT mai2_io4_write_gpio(uint8_t* payload, size_t len);
static HRESULT mai2_io4_write_pwm(uint8_t* payload, size_t len);
static HRESULT mai2_io4_write_unique(uint8_t* payload, size_t len);

static uint16_t coins;

static const struct io4_ops mai2_io4_ops = {
    .poll = mai2_io4_poll,
    .write_gpio = mai2_io4_write_gpio,
    .write_pwm = mai2_io4_write_pwm,
    .write_unique = mai2_io4_write_unique,
};

HRESULT mai2_io4_hook_init(const struct io4_config* cfg) {
    HRESULT hr;

    assert(mai2_dll.init != NULL);

    hr = io4_hook_init(cfg, &mai2_io4_ops, NULL, L"Sinmai", false);

    if (FAILED(hr)) {
        return hr;
    }

    return mai2_dll.init();
}

static HRESULT mai2_io4_poll(void* ctx, struct io4_state* state) {
    uint8_t opbtn;
    uint16_t player1;
    uint16_t player2;
    HRESULT hr;

    assert(mai2_dll.poll != NULL);
    assert(mai2_dll.get_opbtns != NULL);
    assert(mai2_dll.get_gamebtns != NULL);

    memset(state, 0, sizeof(*state));

    hr = mai2_dll.poll();

    if (FAILED(hr)) {
        return hr;
    }

    opbtn = 0;
    player1 = 0;
    player2 = 0;

    mai2_dll.get_opbtns(&opbtn);
    mai2_dll.get_gamebtns(&player1, &player2);

    if (opbtn & MAI2_IO_OPBTN_TEST) {
        state->buttons[0] |= IO4_BUTTON_TEST;
    }

    if (opbtn & MAI2_IO_OPBTN_SERVICE) {
        state->buttons[0] |= IO4_BUTTON_SERVICE;
    }

    if (opbtn & MAI2_IO_OPBTN_COIN) {
        coins++;
    }
    state->chutes[0] = coins << 8;

    // Buttons around screen are active-low, select button is active-high

    // Player 1

    if (!(player1 & MAI2_IO_GAMEBTN_1)) {
        state->buttons[0] |= 1 << 2;
    }

    if (!(player1 & MAI2_IO_GAMEBTN_2)) {
        state->buttons[0] |= 1 << 3;
    }

    if (!(player1 & MAI2_IO_GAMEBTN_3)) {
        state->buttons[0] |= 1 << 0;
    }

    if (!(player1 & MAI2_IO_GAMEBTN_4)) {
        state->buttons[0] |= 1 << 15;
    }

    if (!(player1 & MAI2_IO_GAMEBTN_5)) {
        state->buttons[0] |= 1 << 14;
    }

    if (!(player1 & MAI2_IO_GAMEBTN_6)) {
        state->buttons[0] |= 1 << 13;
    }

    if (!(player1 & MAI2_IO_GAMEBTN_7)) {
        state->buttons[0] |= 1 << 12;
    }

    if (!(player1 & MAI2_IO_GAMEBTN_8)) {
        state->buttons[0] |= 1 << 11;
    }

    if (player1 & MAI2_IO_GAMEBTN_SELECT) {
        state->buttons[0] |= 1 << 1;
    }

    // Player 2

    if (!(player2 & MAI2_IO_GAMEBTN_1)) {
        state->buttons[1] |= 1 << 2;
    }

    if (!(player2 & MAI2_IO_GAMEBTN_2)) {
        state->buttons[1] |= 1 << 3;
    }

    if (!(player2 & MAI2_IO_GAMEBTN_3)) {
        state->buttons[1] |= 1 << 0;
    }

    if (!(player2 & MAI2_IO_GAMEBTN_4)) {
        state->buttons[1] |= 1 << 15;
    }

    if (!(player2 & MAI2_IO_GAMEBTN_5)) {
        state->buttons[1] |= 1 << 14;
    }

    if (!(player2 & MAI2_IO_GAMEBTN_6)) {
        state->buttons[1] |= 1 << 13;
    }

    if (!(player2 & MAI2_IO_GAMEBTN_7)) {
        state->buttons[1] |= 1 << 12;
    }

    if (!(player2 & MAI2_IO_GAMEBTN_8)) {
        state->buttons[1] |= 1 << 11;
    }

    if (player2 & MAI2_IO_GAMEBTN_SELECT) {
        state->buttons[1] |= 1 << 4;
    }

    return S_OK;
}

static HRESULT mai2_io4_write_gpio(uint8_t* payload, size_t len) {
    if (len >= 4) {
        char bin0[9];
        char bin1[9];
        for (int i = 0; i < 8; i++) {
            bin0[7 - i] = (payload[0] & (1 << i)) ? '1' : '0';
            bin1[7 - i] = (payload[1] & (1 << i)) ? '1' : '0';
        }
        bin0[8] = '\0';
        bin1[8] = '\0';

#if defined(LOG_IO4)
        dprintf("IO4 LED: [%02X %02X %02X %02X] (Byte0: %s, Byte1: %s)\n", 
            payload[0], payload[1], payload[2], payload[3], bin0, bin1);

        dprintf("  1P Code Reader: %s\n", (payload[0] & 0x20) ? "ON" : "OFF");
        dprintf("  2P Code Reader: %s\n", (payload[0] & 0x04) ? "ON" : "OFF");
        dprintf("  Camera Ring:    %s\n", (payload[1] & 0x80) ? "ON" : "OFF");
        dprintf("  Camera Rec:     %s\n", (payload[1] & 0x40) ? "ON" : "OFF");
#endif
    }
    return S_OK;
}

static HRESULT mai2_io4_write_pwm(uint8_t* payload, size_t len) {
    if (len >= 16) {
#if defined(LOG_IO4)
        dprintf("IO4 PWM: %02X %02X %02X %02X %02X %02X %02X %02X ...\n", 
            payload[0], payload[1], payload[2], payload[3],
            payload[4], payload[5], payload[6], payload[7]);
#endif
    }
    return S_OK;
}

static HRESULT mai2_io4_write_unique(uint8_t* payload, size_t len) {
    if (len < 8) {
        return S_OK;
    }

#if defined(LOG_IO4)
    dprintf("IO4 Unique: %02X %02X %02X %02X %02X %02X %02X %02X ...\n", 
        payload[0], payload[1], payload[2], payload[3],
        payload[4], payload[5], payload[6], payload[7]);
#endif

    if (mai2_dll.led_set_leds) {
        uint8_t rgb_1p[3] = { payload[2], payload[4], payload[6] };
        uint8_t rgb_2p[3] = { payload[3], payload[5], payload[7] };

        mai2_dll.led_set_leds(0, rgb_1p);
        mai2_dll.led_set_leds(1, rgb_2p);
    }

    return S_OK;
}