#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "amex/jvs.h"

#include "board/io3.h"

#include "kemonohook/kemono-dll.h"

#include "jvs/jvs-bus.h"

#include "util/dprintf.h"

struct kemono_jvs_ir_mask {
    uint16_t p1;
    uint16_t p2;
};

static void kemono_jvs_read_switches(void *ctx, struct io3_switch_state *out);
static void kemono_jvs_read_coin_counter(
        void *ctx,
        uint8_t slot_no,
        uint16_t *out);
static void kemono_jvs_write_gpio(void *ctx, uint32_t state);

static const struct io3_ops kemono_jvs_io3_ops = {
    .read_switches      = kemono_jvs_read_switches,
    .read_coin_counter  = kemono_jvs_read_coin_counter,
    .write_gpio         = kemono_jvs_write_gpio
};

static struct io3 kemono_jvs_io3;

HRESULT kemono_jvs_init(struct jvs_node **out)
{
    HRESULT hr;

    assert(out != NULL);
    assert(kemono_dll.init != NULL);

    dprintf("JVS I/O: Starting IO backend\n");
    hr = kemono_dll.init();

    if (FAILED(hr)) {
        dprintf("JVS I/O: Backend error, I/O disconnected: %x\n", (int) hr);

        return hr;
    }

    io3_init(&kemono_jvs_io3, NULL, &kemono_jvs_io3_ops, NULL);
    *out = io3_to_jvs_node(&kemono_jvs_io3);

    return S_OK;
}

static void kemono_jvs_read_switches(void *ctx, struct io3_switch_state *out)
{
    const struct kemono_jvs_ir_mask *masks;
    uint16_t opbtn;
    uint16_t pbtn;
    size_t i;

    assert(out != NULL);
    assert(kemono_dll.poll != NULL);

    opbtn = 0;
    pbtn = 0;

    kemono_dll.poll(&opbtn, &pbtn);

    out->system = 0x00;
    out->p1 = 0x0000;
    out->p2 = 0x0000;

    if (opbtn & KEMONO_IO_OPBTN_TEST) {
        out->system |= 1 << 7;
    }

    if (opbtn & KEMONO_IO_OPBTN_SERVICE) {
        out->p1 |= 1 << 14;
    }

    if (pbtn & KEMONO_IO_GAMEBTN_UP) {
        out->p1 |= 1 << 13;
    }

    if (pbtn & KEMONO_IO_GAMEBTN_DOWN) {
        out->p1 |= 1 << 12;
    }

    if (pbtn & KEMONO_IO_GAMEBTN_LEFT) {
        out->p1 |= 1 << 11;
    }

    if (pbtn & KEMONO_IO_GAMEBTN_RIGHT) {
        out->p1 |= 1 << 10;
    }

    if (pbtn & KEMONO_IO_GAMEBTN_R) {
        out->p1 |= 1 << 9;
    }

    if (pbtn & KEMONO_IO_GAMEBTN_G) {
        out->p1 |= 1 << 7;
    }

    if (pbtn & KEMONO_IO_GAMEBTN_B) {
        out->p1 |= 1 << 8;
    }

    if (pbtn & KEMONO_IO_GAMEBTN_START) {
        out->p1 |= 1 << 15;
    }


}

static void kemono_jvs_read_coin_counter(
        void *ctx,
        uint8_t slot_no,
        uint16_t *out)
{
    assert(out != NULL);
    assert(kemono_dll.jvs_read_coin_counter != NULL);

    if (slot_no > 0) {
        return;
    }

    kemono_dll.jvs_read_coin_counter(out);
}

static void kemono_jvs_write_gpio(void *ctx, uint32_t state){
    kemono_dll.jvs_write_gpio(state);
}