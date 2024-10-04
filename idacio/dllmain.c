#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "idacio/backend.h"
#include "idacio/config.h"
#include "idacio/di.h"
#include "idacio/idacio.h"
#include "idacio/xi.h"

#include "util/dprintf.h"
#include "util/str.h"

static struct idac_io_config idac_io_cfg;
static const struct idac_io_backend *idac_io_backend;
static bool idac_io_coin;

uint16_t idac_io_get_api_version(void)
{
    return 0x0102;
}

HRESULT idac_io_init(void)
{
    HINSTANCE inst;
    HRESULT hr;

    assert(idac_io_backend == NULL);

    inst = GetModuleHandleW(NULL);

    if (inst == NULL) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        dprintf("GetModuleHandleW failed: %lx\n", hr);

        return hr;
    }

    idac_io_config_load(&idac_io_cfg, L".\\segatools.ini");

    if (wstr_ieq(idac_io_cfg.mode, L"dinput")) {
        hr = idac_di_init(&idac_io_cfg.di, inst, &idac_io_backend);
    } else if (wstr_ieq(idac_io_cfg.mode, L"xinput")) {
        hr = idac_xi_init(&idac_io_cfg.xi, &idac_io_backend);
    } else {
        hr = E_INVALIDARG;
        dprintf("IDAC IO: Invalid IO mode \"%S\", use dinput or xinput\n",
                idac_io_cfg.mode);
    }

    return hr;
}

void idac_io_get_opbtns(uint8_t *opbtn_out)
{
    uint8_t opbtn;

    assert(idac_io_backend != NULL);
    assert(opbtn_out != NULL);

    opbtn = 0;

    /* Common operator buttons, not backend-specific */

    if (GetAsyncKeyState(idac_io_cfg.vk_test) & 0x8000) {
        opbtn |= IDAC_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(idac_io_cfg.vk_service) & 0x8000) {
        opbtn |= IDAC_IO_OPBTN_SERVICE;
    }

    if (GetAsyncKeyState(idac_io_cfg.vk_coin) & 0x8000) {
        if (!idac_io_coin) {
            idac_io_coin = true;
            opbtn |= IDAC_IO_OPBTN_COIN;
        }
    } else {
        idac_io_coin = false;
    }

    *opbtn_out = opbtn;
}


void idac_io_get_gamebtns(uint8_t *gamebtn_out)
{
    assert(idac_io_backend != NULL);
    assert(gamebtn_out != NULL);

    idac_io_backend->get_gamebtns(gamebtn_out);
}

void idac_io_get_shifter(uint8_t *gear)
{
    assert(gear != NULL);
    assert(idac_io_backend != NULL);

    idac_io_backend->get_shifter(gear);
}

void idac_io_get_analogs(struct idac_io_analog_state *out)
{
    struct idac_io_analog_state tmp;

    assert(out != NULL);
    assert(idac_io_backend != NULL);

    idac_io_backend->get_analogs(&tmp);

    /* Apply steering wheel restriction. Real cabs only report about 77% of
       the IO-3's max ADC output value when the wheel is turned to either of
       its maximum positions. To match this behavior we set the default value
       for the wheel restriction config parameter to 97 (out of 128). This
       scaling factor is applied using fixed-point arithmetic below. */

    out->wheel = (tmp.wheel * idac_io_cfg.restrict_) / 128;
    out->accel = tmp.accel;
    out->brake = tmp.brake;
}

HRESULT idac_io_led_init(void)
{
    return S_OK;
}

void idac_io_led_set_fet_output(const uint8_t *rgb)
{
#if 0
    dprintf("IDAC LED: LEFT SEAT LED: %02X\n", rgb[0]);
    dprintf("IDAC LED: RIGHT SEAT LED: %02X\n", rgb[1]);
#endif

    return;
}

void idac_io_led_gs_update(const uint8_t *rgb)
{
#if 0
    for (int i = 0; i < 9; i++) {
        dprintf("IDAC LED: LED %d: %02X %02X %02X Speed: %02X\n",
                i, rgb[i * 4], rgb[i * 4 + 1], rgb[i * 4 + 2], rgb[i * 4 + 3]);
    }
#endif

    return;
}

void idac_io_led_set_leds(const uint8_t *rgb)
{
#if 0
    dprintf("IDAC LED: START: %02X\n", rgb[0]);
    dprintf("IDAC LED: VIEW CHANGE: %02X\n", rgb[1]);
    dprintf("IDAC LED: UP: %02X\n", rgb[2]);
    dprintf("IDAC LED: DOWN: %02X\n", rgb[3]);
    dprintf("IDAC LED: RIGHT: %02X\n", rgb[4]);
    dprintf("IDAC LED: LEFT: %02X\n", rgb[5]);
#endif

    return;
}

HRESULT idac_io_ffb_init(void)
{
    assert(idac_io_backend != NULL);

    return idac_io_backend->ffb_init();
}

void idac_io_ffb_toggle(bool active)
{
    assert(idac_io_backend != NULL);
    
    idac_io_backend->ffb_toggle(active);
}

void idac_io_ffb_constant_force(uint8_t direction, uint8_t force)
{
    assert(idac_io_backend != NULL);
    
    idac_io_backend->ffb_constant_force(direction, force);
}

void idac_io_ffb_rumble(uint8_t period, uint8_t force)
{
    assert(idac_io_backend != NULL);
    
    idac_io_backend->ffb_rumble(period, force);
}

void idac_io_ffb_damper(uint8_t force)
{
    assert(idac_io_backend != NULL);
    
    idac_io_backend->ffb_damper(force);
}
