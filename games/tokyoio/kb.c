#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "tokyoio/backend.h"
#include "tokyoio/config.h"
#include "tokyoio/kb.h"
#include "tokyoio/tokyoio.h"

#include "util/dprintf.h"
#include "util/str.h"

static HRESULT tokyo_kb_config_apply(const struct tokyo_kb_config *cfg);

static void tokyo_kb_get_gamebtns(uint8_t *gamebtn_out);
static void tokyo_kb_get_sensors(uint8_t *sense_out);

static const struct tokyo_io_backend tokyo_kb_backend = {
    .get_gamebtns   = tokyo_kb_get_gamebtns,
    .get_sensors    = tokyo_kb_get_sensors,
};

static struct tokyo_kb_config tokyo_kb_cfg;

HRESULT tokyo_kb_init(const struct tokyo_kb_config *cfg, const struct tokyo_io_backend **backend)
{
    HRESULT hr;

    assert(cfg != NULL);
    assert(backend != NULL);

    hr = tokyo_kb_config_apply(cfg);

    if (FAILED(hr)) {
        return hr;
    }

    dprintf("TokyoIO: Using keyboard input\n");
    *backend = &tokyo_kb_backend;
    return S_OK;
}

static HRESULT tokyo_kb_config_apply(const struct tokyo_kb_config *cfg)
{
    tokyo_kb_cfg = *cfg;

    return S_OK;
}

static void tokyo_kb_get_gamebtns(uint8_t *gamebtn_out)
{
    uint8_t gamebtn;

    assert(gamebtn_out != NULL);

    gamebtn = 0;

    /* PUSH BUTTON inputs */

    if (GetAsyncKeyState(tokyo_kb_cfg.vk_push_left_b) & 0x8000) {
		gamebtn |= TOKYO_IO_GAMEBTN_BLUE;
	}

	if (GetAsyncKeyState(tokyo_kb_cfg.vk_push_center_y) & 0x8000) {
		gamebtn |= TOKYO_IO_GAMEBTN_YELLOW;
	}

    if (GetAsyncKeyState(tokyo_kb_cfg.vk_push_right_r) & 0x8000) {
		gamebtn |= TOKYO_IO_GAMEBTN_RED;
	}

    *gamebtn_out = gamebtn;
}

static void tokyo_kb_get_sensors(uint8_t *sense_out)
{
    uint8_t sense;

    assert(sense_out != NULL);

    sense = 0;

    /* FOOT SENSOR inputs */

    if (GetAsyncKeyState(tokyo_kb_cfg.vk_foot_l) & 0x8000) {
        sense |= TOKYO_IO_SENSE_FOOT_LEFT;
    }

    if (GetAsyncKeyState(tokyo_kb_cfg.vk_foot_r) & 0x8000) {
        sense |= TOKYO_IO_SENSE_FOOT_RIGHT;
    }

    /* JUMP SENSOR inputs */

    if (GetAsyncKeyState(tokyo_kb_cfg.vk_jump_1) & 0x8000) {
        sense |= (TOKYO_IO_SENSE_FOOT_LEFT + TOKYO_IO_SENSE_FOOT_RIGHT +
                  TOKYO_IO_SENSE_JUMP_1);
    }

    if (GetAsyncKeyState(tokyo_kb_cfg.vk_jump_2) & 0x8000) {
        sense |= (TOKYO_IO_SENSE_FOOT_LEFT + TOKYO_IO_SENSE_FOOT_RIGHT +
                  TOKYO_IO_SENSE_JUMP_2);
    }

    if (GetAsyncKeyState(tokyo_kb_cfg.vk_jump_3) & 0x8000) {
        sense |= (TOKYO_IO_SENSE_FOOT_LEFT + TOKYO_IO_SENSE_FOOT_RIGHT +
                  TOKYO_IO_SENSE_JUMP_3);
    }

    if (GetAsyncKeyState(tokyo_kb_cfg.vk_jump_4) & 0x8000) {
        sense |= (TOKYO_IO_SENSE_FOOT_LEFT + TOKYO_IO_SENSE_FOOT_RIGHT +
                  TOKYO_IO_SENSE_JUMP_4);
    }

    if (GetAsyncKeyState(tokyo_kb_cfg.vk_jump_5) & 0x8000) {
        sense |= (TOKYO_IO_SENSE_FOOT_LEFT + TOKYO_IO_SENSE_FOOT_RIGHT +
                  TOKYO_IO_SENSE_JUMP_5);
    }

    if (GetAsyncKeyState(tokyo_kb_cfg.vk_jump_6) & 0x8000) {
        sense |= (TOKYO_IO_SENSE_FOOT_LEFT + TOKYO_IO_SENSE_FOOT_RIGHT +
                  TOKYO_IO_SENSE_JUMP_6);
    }

    if (GetAsyncKeyState(tokyo_kb_cfg.vk_jump_all) & 0x8000) {
        sense |= (TOKYO_IO_SENSE_FOOT_LEFT+ TOKYO_IO_SENSE_FOOT_RIGHT +
                  TOKYO_IO_SENSE_JUMP_1 + TOKYO_IO_SENSE_JUMP_2 +
                  TOKYO_IO_SENSE_JUMP_3 + TOKYO_IO_SENSE_JUMP_4 +
                  TOKYO_IO_SENSE_JUMP_5 + TOKYO_IO_SENSE_JUMP_6);
    }

    *sense_out = sense;
}
