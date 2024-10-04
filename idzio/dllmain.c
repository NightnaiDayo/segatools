#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "idzio/backend.h"
#include "idzio/config.h"
#include "idzio/di.h"
#include "idzio/idzio.h"
#include "idzio/xi.h"

#include "util/dprintf.h"
#include "util/str.h"

static struct idz_io_config idz_io_cfg;
static const struct idz_io_backend *idz_io_backend;
static bool idz_io_coin;
static uint16_t idz_io_coins;

uint16_t idz_io_get_api_version(void)
{
    return 0x0102;
}

HRESULT idz_io_jvs_init(void)
{
    HINSTANCE inst;
    HRESULT hr;

    assert(idz_io_backend == NULL);

    inst = GetModuleHandleW(NULL);

    if (inst == NULL) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        dprintf("GetModuleHandleW failed: %lx\n", hr);

        return hr;
    }

    idz_io_config_load(&idz_io_cfg, L".\\segatools.ini");

    if (wstr_ieq(idz_io_cfg.mode, L"dinput")) {
        hr = idz_di_init(&idz_io_cfg.di, inst, &idz_io_backend);
    } else if (wstr_ieq(idz_io_cfg.mode, L"xinput")) {
        hr = idz_xi_init(&idz_io_cfg.xi, &idz_io_backend);
    } else {
        hr = E_INVALIDARG;
        dprintf("IDZ IO: Invalid IO mode \"%S\", use dinput or xinput\n",
                idz_io_cfg.mode);
    }

    return hr;
}

void idz_io_jvs_read_buttons(uint8_t *opbtn_out, uint8_t *gamebtn_out)
{
    uint8_t opbtn;

    assert(idz_io_backend != NULL);
    assert(opbtn_out != NULL);
    assert(gamebtn_out != NULL);

    opbtn = 0;

    if (GetAsyncKeyState(idz_io_cfg.vk_test) & 0x8000) {
        opbtn |= IDZ_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(idz_io_cfg.vk_service) & 0x8000) {
        opbtn |= IDZ_IO_OPBTN_SERVICE;
    }

    *opbtn_out = opbtn;

    idz_io_backend->jvs_read_buttons(gamebtn_out);
}

void idz_io_jvs_read_shifter(uint8_t *gear)
{
    assert(gear != NULL);
    assert(idz_io_backend != NULL);

    idz_io_backend->jvs_read_shifter(gear);
}

void idz_io_jvs_read_analogs(struct idz_io_analog_state *out)
{
    struct idz_io_analog_state tmp;

    assert(out != NULL);
    assert(idz_io_backend != NULL);

    idz_io_backend->jvs_read_analogs(&tmp);

    /* Apply steering wheel restriction. Real cabs only report about 77% of
       the IO-3's max ADC output value when the wheel is turned to either of
       its maximum positions. To match this behavior we set the default value
       for the wheel restriction config parameter to 97 (out of 128). This
       scaling factor is applied using fixed-point arithmetic below. */

    out->wheel = (tmp.wheel * idz_io_cfg.restrict_) / 128;
    out->accel = tmp.accel;
    out->brake = tmp.brake;
}

void idz_io_jvs_read_coin_counter(uint16_t *out)
{
    assert(out != NULL);

    /* Coin counter is not backend-specific */

    if (idz_io_cfg.vk_coin &&
            (GetAsyncKeyState(idz_io_cfg.vk_coin) & 0x8000)) {
        if (!idz_io_coin) {
            idz_io_coin = true;
            idz_io_coins++;
        }
    } else {
        idz_io_coin = false;
    }

    *out = idz_io_coins;
}

HRESULT idz_io_led_init(void)
{
    return S_OK;
}

void idz_io_led_set_fet_output(const uint8_t *rgb)
{
#if 0
    dprintf("IDZ LED: LEFT SEAT LED: %02X\n", rgb[0]);
    dprintf("IDZ LED: RIGHT SEAT LED: %02X\n", rgb[1]);
#endif

    return;
}

void idz_io_led_gs_update(const uint8_t *rgb)
{
#if 0
    for (int i = 0; i < 9; i++) {
        dprintf("IDZ LED: LED %d: %02X %02X %02X Speed: %02X\n",
                i, rgb[i * 4], rgb[i * 4 + 1], rgb[i * 4 + 2], rgb[i * 4 + 3]);
    }
#endif

    return;
}

void idz_io_led_set_leds(const uint8_t *rgb)
{
#if 0
    dprintf("IDZ LED: START: %02X\n", rgb[0]);
    dprintf("IDZ LED: VIEW CHANGE: %02X\n", rgb[1]);
    dprintf("IDZ LED: UP: %02X\n", rgb[2]);
    dprintf("IDZ LED: DOWN: %02X\n", rgb[3]);
    dprintf("IDZ LED: RIGHT: %02X\n", rgb[4]);
    dprintf("IDZ LED: LEFT: %02X\n", rgb[5]);
#endif

    return;
}

HRESULT idz_io_ffb_init(void)
{
    assert(idz_io_backend != NULL);

    return idz_io_backend->ffb_init();
}

void idz_io_ffb_toggle(bool active)
{
    assert(idz_io_backend != NULL);
    
    idz_io_backend->ffb_toggle(active);
}

void idz_io_ffb_constant_force(uint8_t direction, uint8_t force)
{
    assert(idz_io_backend != NULL);
    
    idz_io_backend->ffb_constant_force(direction, force);
}

void idz_io_ffb_rumble(uint8_t period, uint8_t force)
{
    assert(idz_io_backend != NULL);
    
    idz_io_backend->ffb_rumble(period, force);
}

void idz_io_ffb_damper(uint8_t force)
{
    assert(idz_io_backend != NULL);
    
    idz_io_backend->ffb_damper(force);
}
