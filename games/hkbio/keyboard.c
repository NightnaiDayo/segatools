#include <windows.h>

#include <assert.h>
#include <stdint.h>

#include "hkbio/backend.h"
#include "hkbio/config.h"
#include "hkbio/hkbio.h"
#include "hkbio/keyboard.h"

#include "util/dprintf.h"

static void hkb_kb_get_gamebtns(uint16_t* gamebtn_out);

static const struct hkb_io_backend hkb_kb_backend = {
    .get_gamebtns = hkb_kb_get_gamebtns};

static struct hkb_kb_config config;

HRESULT hkb_kb_init(const struct hkb_kb_config* cfg,
                    const struct hkb_io_backend** backend) {
    assert(cfg != NULL);
    assert(backend != NULL);

    dprintf("Keyboard: Using keyboard input\n");
    *backend = &hkb_kb_backend;
    config = *cfg;

    return S_OK;
}

static void hkb_kb_get_gamebtns(uint16_t* gamebtn_out) {
    assert(gamebtn_out != NULL);

    uint8_t gamebtn = 0;

    if (GetAsyncKeyState(config.vk_terminal_cancel) & 0x8000) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_CANCEL;
    }

    if (GetAsyncKeyState(config.vk_terminal_decide) & 0x8000) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_DECIDE;
    }

    if (GetAsyncKeyState(config.vk_terminal_up) & 0x8000) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_UP;
    }

    if (GetAsyncKeyState(config.vk_terminal_right) & 0x8000) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_RIGHT;
    }

    if (GetAsyncKeyState(config.vk_terminal_down) & 0x8000) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_DOWN;
    }

    if (GetAsyncKeyState(config.vk_terminal_left) & 0x8000) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_LEFT;
    }

    if (GetAsyncKeyState(config.vk_terminal_left_2) & 0x8000) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_LEFT_2;
    }

    if (GetAsyncKeyState(config.vk_terminal_right_2) & 0x8000) {
        gamebtn |= HKB_IO_GAMEBTN_TERMINAL_RIGHT_2;
    }

    *gamebtn_out = gamebtn;
}