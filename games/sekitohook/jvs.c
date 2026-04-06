#include "jvs.h"

#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "amex/jvs.h"

#include "board/io3.h"

#include "sekitohook/config.h"

#include "jvs/jvs-bus.h"

#include "util/dprintf.h"

static void sekito_jvs_read_switches(void* ctx, struct io3_switch_state* out);

static void sekito_jvs_read_coin_counter(
    void* ctx,
    uint8_t slot_no,
    uint16_t* out);

static void sekito_jvs_read_rotary(
    void* ctx,
    uint16_t* rotary,
    uint8_t nrotary);

static const struct io3_ops sekito_jvs_io3_ops = {
    .read_switches = sekito_jvs_read_switches,
    .read_rotarys = sekito_jvs_read_rotary,
    .read_coin_counter = sekito_jvs_read_coin_counter,
};

static struct io3 sekito_jvs_io3;

static bool io_is_terminal;

HRESULT sekito_jvs_init(struct jvs_node** out) {
    assert(out != NULL);

    dprintf("JVS I/O: Starting JVS\n");

    io3_init(&sekito_jvs_io3, NULL, &sekito_jvs_io3_ops, NULL);
    *out = io3_to_jvs_node(&sekito_jvs_io3);

    return S_OK;
}

void sekito_jvs_set_terminal(bool is_terminal) {
    dprintf("JVS I/O: Terminal: %d\n", is_terminal);
    io_is_terminal = is_terminal;
}

static void sekito_jvs_read_switches(void* ctx, struct io3_switch_state* out) {
    assert(out != NULL);

    uint8_t opbtn;
    uint32_t gamebtn;

    assert(sekito_dll.poll != NULL);
    assert(sekito_dll.get_opbtns != NULL);
    assert(sekito_dll.get_gamebtns != NULL);

    memset(out, 0, sizeof(*out));

    sekito_dll.poll();

    opbtn = 0;
    gamebtn = 0;

    sekito_dll.get_opbtns(&opbtn);
    sekito_dll.get_gamebtns(&gamebtn);

    if (opbtn & SEKITO_IO_OPBTN_TEST) {
        out->system = 0x80;
    } else {
        out->system = 0;
    }

    if (opbtn & SEKITO_IO_OPBTN_SERVICE) {
        out->p1 |= 1 << 1;
    }

    if (opbtn & SEKITO_IO_OPBTN_SW1) {
        out->p1 |= 1 << 10;
    }

    if (opbtn & SEKITO_IO_OPBTN_SW2) {
        out->p1 |= 1 << 11;
    }

    if (opbtn & SEKITO_IO_OPBTN_COIN) {
        out->p1 |= 1 << 14;
    }


    if (!io_is_terminal) {
        if (gamebtn & SEKITO_IO_GAMEBTN_HOUGU) {
            out->p2 |= 1 << 6;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_MENU) {
            out->p2 |= 1 << 4;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_START) {
            out->p1 |= 1 << 15;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_STRATAGEM) {
            out->p2 |= 1 << 7;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_STRATAGEM_LOCK) {
            out->p2 |= 1 << 5;
        }

        out->p1 |= 0 << 2; // card_sensor
        out->p2 |= 1 << 2; // open_sensor
        out->p2 |= 1 << 3; // lock
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_0) {
        out->p1 |= SEKITO_NUMPAD_C2;
        out->p1 |= SEKITO_NUMPAD_R4;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_1) {
        out->p1 |= SEKITO_NUMPAD_C1;
        out->p1 |= SEKITO_NUMPAD_R1;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_2) {
        out->p1 |= SEKITO_NUMPAD_C2;
        out->p1 |= SEKITO_NUMPAD_R1;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_3) {
        out->p1 |= SEKITO_NUMPAD_C3;
        out->p1 |= SEKITO_NUMPAD_R1;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_4) {
        out->p1 |= SEKITO_NUMPAD_C1;
        out->p1 |= SEKITO_NUMPAD_R2;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_5) {
        out->p1 |= SEKITO_NUMPAD_C2;
        out->p1 |= SEKITO_NUMPAD_R2;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_6) {
        out->p1 |= SEKITO_NUMPAD_C3;
        out->p1 |= SEKITO_NUMPAD_R2;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_7) {
        out->p1 |= SEKITO_NUMPAD_C1;
        out->p1 |= SEKITO_NUMPAD_R3;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_8) {
        out->p1 |= SEKITO_NUMPAD_C2;
        out->p1 |= SEKITO_NUMPAD_R3;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_9) {
        out->p1 |= SEKITO_NUMPAD_C3;
        out->p1 |= SEKITO_NUMPAD_R3;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_CLEAR) {
        out->p1 |= SEKITO_NUMPAD_C1;
        out->p1 |= SEKITO_NUMPAD_R4;
    }

    if (gamebtn & SEKITO_IO_GAMEBTN_NUMPAD_ENTER) {
        out->p1 |= SEKITO_NUMPAD_C3;
        out->p1 |= SEKITO_NUMPAD_R4;
    }

    if (io_is_terminal) {
        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_CANCEL) {
            out->p2 |= 1 << 8;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_DECIDE) {
            out->p2 |= 1 << 9;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_LEFT) {
            out->p1 |= 1 << 11;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_UP) {
            out->p1 |= 1 << 13;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_RIGHT) {
            out->p1 |= 1 << 10;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_DOWN) {
            out->p1 |= 1 << 12;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_LEFT_2) {
            out->p2 |= 1 << 11;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_RIGHT_2) {
            out->p2 |= 1 << 10;
        }

        if (gamebtn & SEKITO_IO_GAMEBTN_TERMINAL_RESERVE) {
            out->p2 |= 1 << 3;
        }

    }

}

static void sekito_jvs_read_rotary(
    void* ctx,
    uint16_t* rotary,
    uint8_t nrotary) {
    assert(rotary != NULL);
    assert(sekito_dll.get_trackball_position != NULL);

    uint16_t x, y;

    x = 0;
    y = 0;

    sekito_dll.get_trackball_position(&x, &y);

    if (nrotary >= 4) {
        rotary[2] = x;
        rotary[3] = y;
    }
}

static void sekito_jvs_read_coin_counter(
    void* ctx,
    uint8_t slot_no,
    uint16_t* out) {
    if (slot_no > 0) {
        return;
    }

    // unused(!)
    *out = 0;
}
