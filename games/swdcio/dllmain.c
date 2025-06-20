#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "swdcio/backend.h"
#include "swdcio/config.h"
#include "swdcio/di.h"
#include "swdcio/swdcio.h"
#include "swdcio/xi.h"

#include "util/dprintf.h"
#include "util/str.h"
#include "util/env.h"

static struct swdc_io_config swdc_io_cfg;
static const struct swdc_io_backend *swdc_io_backend;
static bool swdc_io_coin;

uint16_t swdc_io_get_api_version(void)
{
    return 0x0102;
}

HRESULT swdc_io_init(void)
{
    HINSTANCE inst;
    HRESULT hr;

    assert(swdc_io_backend == NULL);

    inst = GetModuleHandleW(NULL);

    if (inst == NULL) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        dprintf("GetModuleHandleW failed: %lx\n", hr);

        return hr;
    }

    swdc_io_config_load(&swdc_io_cfg, get_config_path());

    if (wstr_ieq(swdc_io_cfg.mode, L"dinput")) {
        hr = swdc_di_init(&swdc_io_cfg.di, inst, &swdc_io_backend);
    } else if (wstr_ieq(swdc_io_cfg.mode, L"xinput")) {
        hr = swdc_xi_init(&swdc_io_cfg.xi, &swdc_io_backend);
    } else {
        hr = E_INVALIDARG;
        dprintf("swdc IO: Invalid IO mode \"%S\", use dinput or xinput\n",
                swdc_io_cfg.mode);
    }

    return hr;
}

void swdc_io_get_opbtns(uint8_t *opbtn_out)
{
    uint8_t opbtn;

    assert(swdc_io_backend != NULL);
    assert(opbtn_out != NULL);

    opbtn = 0;

    /* Common operator buttons, not backend-specific */

    if (GetAsyncKeyState(swdc_io_cfg.vk_test) & 0x8000) {
        opbtn |= SWDC_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(swdc_io_cfg.vk_service) & 0x8000) {
        opbtn |= SWDC_IO_OPBTN_SERVICE;
    }

    if (GetAsyncKeyState(swdc_io_cfg.vk_coin) & 0x8000) {
        if (!swdc_io_coin) {
            swdc_io_coin = true;
            opbtn |= SWDC_IO_OPBTN_COIN;
        }
    } else {
        swdc_io_coin = false;
    }

    *opbtn_out = opbtn;
}


void swdc_io_get_gamebtns(uint16_t *gamebtn_out)
{
    assert(swdc_io_backend != NULL);
    assert(gamebtn_out != NULL);

    swdc_io_backend->get_gamebtns(gamebtn_out);
}

void swdc_io_get_analogs(struct swdc_io_analog_state *out)
{
    struct swdc_io_analog_state tmp;

    assert(out != NULL);
    assert(swdc_io_backend != NULL);

    swdc_io_backend->get_analogs(&tmp);

    /* Apply steering wheel restriction. Real cabs only report about 77% of
       the IO-3's max ADC output value when the wheel is turned to either of
       its maximum positions. To match this behavior we set the default value
       for the wheel restriction config parameter to 97 (out of 128). This
       scaling factor is applied using fixed-point arithmetic below. */

    out->wheel = (tmp.wheel * swdc_io_cfg.restrict_) / 128;
    out->accel = tmp.accel;
    out->brake = tmp.brake;
}

HRESULT swdc_io_led_init(void)
{
    return S_OK;
}

void swdc_io_led_set_fet_output(uint8_t board, const uint8_t *rgb)
{
#if 0
    dprintf("SWDC LED: LEFT SEAT LED: %02X\n", rgb[0]);
    dprintf("SWDC LED: RIGHT SEAT LED: %02X\n", rgb[1]);
#endif

    return;
}

void swdc_io_led_gs_update(uint8_t board, const uint8_t *rgb)
{
#if 0
    for (int i = 0; i < 9; i++) {
        dprintf("SWDC LED: LED %d: %02X %02X %02X Speed: %02X\n",
                i, rgb[i * 4], rgb[i * 4 + 1], rgb[i * 4 + 2], rgb[i * 4 + 3]);
    }
#endif

    return;
}

void swdc_io_led_set_leds(uint8_t board, const uint8_t *rgb)
{
#if 0
    dprintf("SWDC LED: START: %02X\n", rgb[0]);
    dprintf("SWDC LED: VIEW CHANGE: %02X\n", rgb[1]);
    dprintf("SWDC LED: UP: %02X\n", rgb[2]);
    dprintf("SWDC LED: DOWN: %02X\n", rgb[3]);
    dprintf("SWDC LED: RIGHT: %02X\n", rgb[4]);
    dprintf("SWDC LED: LEFT: %02X\n", rgb[5]);
#endif

    return;
}

HRESULT swdc_io_ffb_init(void)
{
    assert(swdc_io_backend != NULL);

    return swdc_io_backend->ffb_init();
}

void swdc_io_ffb_toggle(bool active)
{
    assert(swdc_io_backend != NULL);
    
    swdc_io_backend->ffb_toggle(active);
}

void swdc_io_ffb_constant_force(uint8_t direction, uint8_t force)
{
    assert(swdc_io_backend != NULL);
    
    swdc_io_backend->ffb_constant_force(direction, force);
}

void swdc_io_ffb_rumble(uint8_t period, uint8_t force)
{
    assert(swdc_io_backend != NULL);
    
    swdc_io_backend->ffb_rumble(period, force);
}

void swdc_io_ffb_damper(uint8_t force)
{
    assert(swdc_io_backend != NULL);
    
    swdc_io_backend->ffb_damper(force);
}
