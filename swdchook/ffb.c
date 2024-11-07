#include <windows.h>

#include <xinput.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "board/ffb.h"

#include "swdchook/swdc-dll.h"

#include "util/dprintf.h"

static void swdc_ffb_toggle(bool active);
static void swdc_ffb_constant_force(uint8_t direction, uint8_t force);
static void swdc_ffb_rumble(uint8_t force, uint8_t period);
static void swdc_ffb_damper(uint8_t force);

static const struct ffb_ops swdc_ffb_ops = {
    .toggle         = swdc_ffb_toggle,
    .constant_force = swdc_ffb_constant_force,
    .rumble         = swdc_ffb_rumble,
    .damper         = swdc_ffb_damper
};

HRESULT swdc_ffb_hook_init(const struct ffb_config *cfg, unsigned int port_no)
{
    HRESULT hr;

    assert(swdc_dll.init != NULL);

    hr = ffb_hook_init(cfg, &swdc_ffb_ops, port_no);

    if (FAILED(hr)) {
        return hr;
    }

    return swdc_dll.ffb_init();
}

static void swdc_ffb_toggle(bool active)
{
    swdc_dll.ffb_toggle(active);
}

static void swdc_ffb_constant_force(uint8_t direction, uint8_t force)
{
    swdc_dll.ffb_constant_force(direction, force);
}

static void swdc_ffb_rumble(uint8_t force, uint8_t period)
{
    swdc_dll.ffb_rumble(force, period);
}

static void swdc_ffb_damper(uint8_t force)
{
    swdc_dll.ffb_damper(force);
}
