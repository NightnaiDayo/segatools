#include <windows.h>

#include <assert.h>
#include <stdlib.h>

#include "swdchook/swdc-dll.h"

#include "util/dll-bind.h"
#include "util/dprintf.h"

const struct dll_bind_sym swdc_dll_syms[] = {
    {
        .sym = "swdc_io_init",
        .off = offsetof(struct swdc_dll, init),
    }, {
        .sym = "swdc_io_get_opbtns",
        .off = offsetof(struct swdc_dll, get_opbtns),
    }, {
        .sym = "swdc_io_get_gamebtns",
        .off = offsetof(struct swdc_dll, get_gamebtns),
    }, {
        .sym = "swdc_io_get_analogs",
        .off = offsetof(struct swdc_dll, get_analogs),
    }, {
        .sym = "swdc_io_led_init",
        .off = offsetof(struct swdc_dll, led_init),
    }, {
        .sym = "swdc_io_led_set_fet_output",
        .off = offsetof(struct swdc_dll, led_set_fet_output),
    }, {
        .sym = "swdc_io_led_gs_update",
        .off = offsetof(struct swdc_dll, led_gs_update),
    }, {
        .sym = "swdc_io_led_set_leds",
        .off = offsetof(struct swdc_dll, led_set_leds),
    }, {
        .sym = "swdc_io_ffb_init",
        .off = offsetof(struct swdc_dll, ffb_init),
    }, {
        .sym = "swdc_io_ffb_toggle",
        .off = offsetof(struct swdc_dll, ffb_toggle),
    }, {
        .sym = "swdc_io_ffb_constant_force",
        .off = offsetof(struct swdc_dll, ffb_constant_force),
    }, {
        .sym = "swdc_io_ffb_rumble",
        .off = offsetof(struct swdc_dll, ffb_rumble),
    }, {
        .sym = "swdc_io_ffb_damper",
        .off = offsetof(struct swdc_dll, ffb_damper),
    }
};

struct swdc_dll swdc_dll;

// Copypasta DLL binding and diagnostic message boilerplate.
// Not much of this lends itself to being easily factored out. Also there
// will be a lot of API-specific branching code here eventually as new API
// versions get defined, so even though these functions all look the same
// now this won't remain the case forever.

HRESULT swdc_dll_init(const struct swdc_dll_config *cfg, HINSTANCE self)
{
    uint16_t (*get_api_version)(void);
    const struct dll_bind_sym *sym;
    HINSTANCE owned;
    HINSTANCE src;
    HRESULT hr;

    assert(cfg != NULL);
    assert(self != NULL);

    if (cfg->path[0] != L'\0') {
        owned = LoadLibraryW(cfg->path);

        if (owned == NULL) {
            hr = HRESULT_FROM_WIN32(GetLastError());
            dprintf("SWDC IO: Failed to load IO DLL: %lx: %S\n",
                    hr,
                    cfg->path);

            goto end;
        }

        dprintf("SWDC IO: Using custom IO DLL: %S\n", cfg->path);
        src = owned;
    } else {
        owned = NULL;
        src = self;
    }

    get_api_version = (void *) GetProcAddress(src, "swdc_io_get_api_version");

    if (get_api_version != NULL) {
        swdc_dll.api_version = get_api_version();
    } else {
        swdc_dll.api_version = 0x0100;
        dprintf("Custom IO DLL does not expose swdc_io_get_api_version, "
                "assuming API version 1.0.\n"
                "Please ask the developer to update their DLL.\n");
    }

    if (swdc_dll.api_version >= 0x0200) {
        hr = E_NOTIMPL;
        dprintf("SWDC IO: Custom IO DLL implements an unsupported "
                "API version (%#04x). Please update Segatools.\n",
                swdc_dll.api_version);

        goto end;
    }

    sym = swdc_dll_syms;
    hr = dll_bind(&swdc_dll, src, &sym, _countof(swdc_dll_syms));

    if (FAILED(hr)) {
        if (src != self) {
            dprintf("SWDC IO: Custom IO DLL does not provide function "
                    "\"%s\". Please contact your IO DLL's developer for "
                    "further assistance.\n",
                    sym->sym);

            goto end;
        } else {
            dprintf("Internal error: could not reflect \"%s\"\n", sym->sym);
        }
    }

    owned = NULL;

end:
    if (owned != NULL) {
        FreeLibrary(owned);
    }

    return hr;
}
