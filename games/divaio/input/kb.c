#include <windows.h>

#include <assert.h>
#include <stdint.h>

#include "divaio/config.h"
#include "divaio/divaio.h"
#include "divaio/input/backend.h"
#include "divaio/input/kb.h"

#include "util/dprintf.h"

static uint8_t diva_kb_start;
static uint8_t diva_kb_circle;
static uint8_t diva_kb_cross;
static uint8_t diva_kb_square;
static uint8_t diva_kb_triangle;
static uint8_t diva_kb_auto_left;
static uint8_t diva_kb_auto_right;
static uint8_t diva_kb_slider[DIVA_SLIDER_CELL_COUNT];

static void diva_kb_get_gamebtns(struct diva_button_states* states);
static void diva_kb_get_auto_status(bool* l, bool* r);
static void diva_kb_get_slider(uint32_t* slider);

static HRESULT diva_kb_config_apply(const struct diva_kb_config* cfg);

static const struct diva_io_backend diva_kb_backend = {
    .get_gamebtns    = diva_kb_get_gamebtns,
    .get_slider      = diva_kb_get_slider,
    .get_auto_status = diva_kb_get_auto_status,
};

HRESULT diva_kb_init(const struct diva_kb_config* cfg,
                     const struct diva_io_backend** backend) {
    assert(cfg != NULL);
    assert(backend != NULL);

    *backend = NULL;

    HRESULT hr = diva_kb_config_apply(cfg);

    if (FAILED(hr)) {
        return hr;
    }

    dprintf("Diva IO: Using keyboard input\n");
    *backend = &diva_kb_backend;

    return S_OK;
}

static HRESULT diva_kb_config_apply(const struct diva_kb_config* cfg) {
    if (cfg->vk_start > 255) {
        dprintf("Diva IO: Invalid start key configuration: %u\n", cfg->vk_start);
        return E_INVALIDARG;
    }

    if (cfg->vk_cross > 255) {
        dprintf("Diva IO: Invalid cross key configuration: %u\n", cfg->vk_cross);
        return E_INVALIDARG;
    }

    if (cfg->vk_circle > 255) {
        dprintf("Diva IO: Invalid circle key configuration: %u\n", cfg->vk_circle);
        return E_INVALIDARG;
    }

    if (cfg->vk_square > 255) {
        dprintf("Diva IO: Invalid square key configuration: %u\n", cfg->vk_square);
        return E_INVALIDARG;
    }

    if (cfg->vk_triangle > 255) {
        dprintf("Diva IO: Invalid triangle key configuration: %u\n", cfg->vk_triangle);
        return E_INVALIDARG;
    }

    if (cfg->vk_auto_left > 255) {
        dprintf("Diva IO: Invalid auto slide left key configuration: %u\n", cfg->vk_auto_left);
        return E_INVALIDARG;
    }

    if (cfg->vk_auto_right > 255) {
        dprintf("Diva IO: Invalid auto slide right key configuration: %u\n", cfg->vk_auto_right);
        return E_INVALIDARG;
    }

    for (int i = 0; i < DIVA_SLIDER_CELL_COUNT; i++) {
        if (cfg->vk_slider[i] > 255) {
            dprintf("Diva IO: Invalid auto slider cell %d key configuration: %u\n", i+ 1, cfg->vk_slider[i]);
            return E_INVALIDARG;
        }
    }

    /* Apply the configuration */
    diva_kb_start = cfg->vk_start;
    diva_kb_circle = cfg->vk_circle;
    diva_kb_cross = cfg->vk_cross;
    diva_kb_triangle = cfg->vk_triangle;
    diva_kb_square = cfg->vk_square;
    diva_kb_auto_left = cfg->vk_auto_left;
    diva_kb_auto_right = cfg->vk_auto_right;

    for (int i = 0; i < DIVA_SLIDER_CELL_COUNT; i++) {
        diva_kb_slider[i] = cfg->vk_slider[i];
    }

    /* Print some debug output to make sure config works... */
    dprintf("Diva IO: --- Begin configuration ---\n");
    dprintf("Diva IO: Start key  . . . : %u\n", diva_kb_start);
    dprintf("Diva IO: Square key . . . : %u\n", diva_kb_square);
    dprintf("Diva IO: Triangle key . . : %u\n", diva_kb_triangle);
    dprintf("Diva IO: Cross key  . . . : %u\n", diva_kb_cross);
    dprintf("Diva IO: Circle key . . . : %u\n", diva_kb_circle);
    dprintf("Diva IO: Left slide key . : %u\n", diva_kb_auto_left);
    dprintf("Diva IO: Right slide key  : %u\n", diva_kb_auto_right);

    /* Print the configuration for all 8 buttons */
    for (int i = 0; i < DIVA_SLIDER_CELL_COUNT; i++) {
        dprintf("Diva IO: Slider %02i  . . . : %u\n", i + 1, diva_kb_slider[i]);
    }

    dprintf("Diva IO: --- End configuration ---\n");

    return S_OK;
}

static void diva_kb_get_gamebtns(struct diva_button_states* states) {
    assert(states != NULL);

    states->start = GetAsyncKeyState(diva_kb_start) & 0x8000;
    states->cross.primary = GetAsyncKeyState(diva_kb_cross) & 0x8000;
    states->square.primary = GetAsyncKeyState(diva_kb_square) & 0x8000;
    states->triangle.primary = GetAsyncKeyState(diva_kb_triangle) & 0x8000;
    states->circle.primary = GetAsyncKeyState(diva_kb_circle) & 0x8000;

}

static void diva_kb_get_auto_status(bool* l, bool* r) {
    assert(l != NULL);
    assert(r != NULL);

    *l = GetAsyncKeyState(diva_kb_auto_left) & 0x8000;
    *r = GetAsyncKeyState(diva_kb_auto_right) & 0x8000;
}

void diva_kb_get_slider(uint32_t* slider) {
    assert(slider != NULL);

    for (int i = 0; i < DIVA_SLIDER_CELL_COUNT; i++) {
        if (GetAsyncKeyState(diva_kb_slider[i]) & 0x8000) {
            *slider |= 1 << i;
        }
    }
}
