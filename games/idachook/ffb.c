#include <windows.h>

#include <xinput.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "board/ffb.h"

#include "idachook/idac-dll.h"

#include "util/dprintf.h"

static void idac_ffb_toggle(bool active);
static void idac_ffb_constant_force(uint8_t direction, uint8_t force);
static void idac_ffb_rumble(uint8_t force, uint8_t period);
static void idac_ffb_damper(uint8_t force);

static const struct ffb_ops idac_ffb_ops = {
    .toggle         = idac_ffb_toggle,
    .constant_force = idac_ffb_constant_force,
    .rumble         = idac_ffb_rumble,
    .damper         = idac_ffb_damper
};

HRESULT idac_ffb_hook_init(const struct ffb_config *cfg, unsigned int port_no)
{
    HRESULT hr;

    assert(idac_dll.init != NULL);

    hr = ffb_hook_init(cfg, &idac_ffb_ops, port_no);

    if (FAILED(hr)) {
        return hr;
    }

    return idac_dll.ffb_init();
}

static void idac_ffb_toggle(bool active)
{
    idac_dll.ffb_toggle(active);
}

static void idac_ffb_constant_force(uint8_t direction, uint8_t force)
{
    idac_dll.ffb_constant_force(direction, force);
}

static void idac_ffb_rumble(uint8_t force, uint8_t period)
{
    idac_dll.ffb_rumble(force, period);
}

static void idac_ffb_damper(uint8_t force)
{
    idac_dll.ffb_damper(force);
}
