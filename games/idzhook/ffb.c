#include <windows.h>

#include <xinput.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "board/ffb.h"

#include "idzhook/idz-dll.h"

#include "util/dprintf.h"

static void idz_ffb_toggle(bool active);
static void idz_ffb_constant_force(uint8_t direction, uint8_t force);
static void idz_ffb_rumble(uint8_t force, uint8_t period);
static void idz_ffb_damper(uint8_t force);

static const struct ffb_ops idz_ffb_ops = {
    .toggle         = idz_ffb_toggle,
    .constant_force = idz_ffb_constant_force,
    .rumble         = idz_ffb_rumble,
    .damper         = idz_ffb_damper
};

HRESULT idz_ffb_hook_init(const struct ffb_config *cfg, unsigned int port_no)
{
    HRESULT hr;

    assert(idz_dll.jvs_init != NULL);

    hr = ffb_hook_init(cfg, &idz_ffb_ops, port_no);

    if (FAILED(hr)) {
        return hr;
    }

    return idz_dll.ffb_init();
}

static void idz_ffb_toggle(bool active)
{
    idz_dll.ffb_toggle(active);
}

static void idz_ffb_constant_force(uint8_t direction, uint8_t force)
{
    idz_dll.ffb_constant_force(direction, force);
}

static void idz_ffb_rumble(uint8_t force, uint8_t period)
{
    idz_dll.ffb_rumble(force, period);
}

static void idz_ffb_damper(uint8_t force)
{
    idz_dll.ffb_damper(force);
}
