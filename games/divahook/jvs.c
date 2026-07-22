#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "amex/jvs.h"

#include "board/io3.h"

#include "divahook/diva-dll.h"

#include "jvs/jvs-bus.h"

#include "util/dprintf.h"
#include "util/fg-detect.h"

static void diva_jvs_read_switches(void *ctx, struct io3_switch_state *out);
static void diva_jvs_read_coin_counter(
        void *ctx,
        uint8_t slot_no,
        uint16_t *out);
static void diva_jvs_write_gpio(void *ctx, uint32_t state);

static const struct io3_ops diva_jvs_io3_ops = {
    .read_switches      = diva_jvs_read_switches,
    .read_coin_counter  = diva_jvs_read_coin_counter,
    .write_gpio         = diva_jvs_write_gpio
};

static struct io3 diva_jvs_io3;

HRESULT diva_jvs_init(struct jvs_node **out)
{
    HRESULT hr;

    assert(out != NULL);
    assert(diva_dll.jvs_init != NULL);

    dprintf("JVS I/O: Starting Diva backend DLL\n");
    hr = diva_dll.jvs_init();

    if (FAILED(hr)) {
        dprintf("JVS I/O: Backend error, I/O disconnected: %x\n", (int) hr);

        return hr;
    }

    io3_init(&diva_jvs_io3, NULL, &diva_jvs_io3_ops, NULL);
    *out = io3_to_jvs_node(&diva_jvs_io3);

    return S_OK;
}

static void diva_jvs_read_switches(void *ctx, struct io3_switch_state *out)
{
    uint8_t opbtn;
    uint8_t gamebtn;

    assert(out != NULL);
    assert(diva_dll.jvs_poll != NULL);

    opbtn = 0;
    gamebtn = 0;

    if (!fgdet_in_foreground()) {
        return;
    }

    diva_dll.jvs_poll(&opbtn, &gamebtn);

    if (gamebtn & DIVA_IO_GAMEBTN_CIRCLE) {
        out->p1 |= 1 << 6;
    }

    if (gamebtn & DIVA_IO_GAMEBTN_CROSS) {
        out->p1 |= 1 << 7;
    }

    if (gamebtn & DIVA_IO_GAMEBTN_SQUARE) {
        out->p1 |= 1 << 8;
    }

    if (gamebtn & DIVA_IO_GAMEBTN_TRIANGLE) {
        out->p1 |= 1 << 9;
    }

    if (gamebtn & DIVA_IO_GAMEBTN_START) {
        out->p1 |= 1 << 15;
    }

    if (opbtn & DIVA_IO_OPBTN_TEST) {
        out->system = 0x80;
    } else {
        out->system = 0;
    }

    if (opbtn & DIVA_IO_OPBTN_SERVICE) {
        out->p1 |= 1 << 14;
    }
}

static void diva_jvs_read_coin_counter(
        void *ctx,
        uint8_t slot_no,
        uint16_t *out)
{
    assert(diva_dll.jvs_read_coin_counter != NULL);

    if (slot_no > 0) {
        return;
    }

    diva_dll.jvs_read_coin_counter(out);
}

static void diva_jvs_write_gpio(void *ctx, uint32_t state) 
{
    assert(diva_dll.led_set_leds != NULL);
    
    // Since Sega uses an odd ordering for the first part of the bitfield,
    // let's normalize the data and just send over bytes for the receiver
    // to interpret as ON/OFF values.
    uint8_t rgb_out[10] = {
        state & DIVA_IO_LED_LEFT_PARTITION_RED    ? 0xFF : 0x00,
        state & DIVA_IO_LED_LEFT_PARTITION_GREEN  ? 0xFF : 0x00,
        state & DIVA_IO_LED_LEFT_PARTITION_BLUE   ? 0xFF : 0x00,
        state & DIVA_IO_LED_RIGHT_PARTITION_RED   ? 0xFF : 0x00,
        state & DIVA_IO_LED_RIGHT_PARTITION_GREEN ? 0xFF : 0x00,
        state & DIVA_IO_LED_RIGHT_PARTITION_BLUE  ? 0xFF : 0x00,
        state & DIVA_IO_LED_BTN_TRIANGLE          ? 0xFF : 0x00,
        state & DIVA_IO_LED_BTN_CROSS             ? 0xFF : 0x00,
        state & DIVA_IO_LED_BTN_SQUARE            ? 0xFF : 0x00,
        state & DIVA_IO_LED_BTN_CIRCLE            ? 0xFF : 0x00
    };

    diva_dll.led_set_leds(0, rgb_out);
}
