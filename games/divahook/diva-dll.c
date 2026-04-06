#include <windows.h>

#include <assert.h>
#include <stdlib.h>

#include "divahook/diva-dll.h"

#include "util/dll-bind.h"
#include "util/dprintf.h"

enum {
    DIVA_DLL_SYM_COUNT_V100 = 7,
    DIVA_DLL_SYM_COUNT_V101 = 12,
};

const struct dll_bind_sym diva_dll_syms[] = {
    {
        .sym = "diva_io_jvs_init",
        .off = offsetof(struct diva_dll, jvs_init),
    }, {
        .sym = "diva_io_jvs_poll",
        .off = offsetof(struct diva_dll, jvs_poll),
    }, {
        .sym = "diva_io_jvs_read_coin_counter",
        .off = offsetof(struct diva_dll, jvs_read_coin_counter),
    }, {
        .sym = "diva_io_slider_init",
        .off = offsetof(struct diva_dll, slider_init),
    }, {
        .sym = "diva_io_slider_start",
        .off = offsetof(struct diva_dll, slider_start),
    }, {
        .sym = "diva_io_slider_stop",
        .off = offsetof(struct diva_dll, slider_stop),
    }, {
        .sym = "diva_io_slider_set_leds",
        .off = offsetof(struct diva_dll, slider_set_leds),
    }, {
        .sym = "diva_io_led_init",
        .off = offsetof(struct diva_dll, led_init),
    }, {
        .sym = "diva_io_led_set_leds",
        .off = offsetof(struct diva_dll, led_set_leds),
    }, {
        .sym = "diva_io_touch_init",
        .off = offsetof(struct diva_dll, touch_init),
    }, {
        .sym = "diva_io_touch_start",
        .off = offsetof(struct diva_dll, touch_start),
    }, {
        .sym = "diva_io_touch_stop",
        .off = offsetof(struct diva_dll, touch_stop),
    }
};

struct diva_dll diva_dll;

// Copypasta DLL binding and diagnostic message boilerplate.
// Not much of this lends itself to being easily factored out. Also there
// will be a lot of API-specific branching code here eventually as new API
// versions get defined, so even though these functions all look the same
// now this won't remain the case forever.

HRESULT diva_dll_init(const struct diva_dll_config *cfg, HINSTANCE self)
{
    uint16_t (*get_api_version)(void);
    const struct dll_bind_sym *sym;
    HINSTANCE owned;
    HINSTANCE src;
    HRESULT hr;
    size_t sym_count;

    assert(cfg != NULL);
    assert(self != NULL);

    if (cfg->path[0] != L'\0') {
        owned = LoadLibraryW(cfg->path);

        if (owned == NULL) {
            hr = HRESULT_FROM_WIN32(GetLastError());
            dprintf("Diva IO: Failed to load IO DLL: %lx: %S\n",
                    hr,
                    cfg->path);

            goto end;
        }

        dprintf("Diva IO: Using custom IO DLL: %S\n", cfg->path);
        src = owned;
    } else {
        owned = NULL;
        src = self;
    }

    get_api_version = (void *) GetProcAddress(src, "diva_io_get_api_version");

    if (get_api_version != NULL) {
        diva_dll.api_version = get_api_version();
    } else {
        diva_dll.api_version = 0x0100;
        dprintf("Custom IO DLL does not expose diva_io_get_api_version, "
                "assuming API version 1.0.\n"
                "Please ask the developer to update their DLL.\n");
    }

    if (diva_dll.api_version >= 0x0200) {
        hr = E_NOTIMPL;
        dprintf("Diva IO: Custom IO DLL implements an unsupported "
                "API version (%#04x). Please update Segatools.\n",
                diva_dll.api_version);

        goto end;
    }

    sym = diva_dll_syms;
    switch (diva_dll.api_version) {
    case 0x0101:
        sym_count = DIVA_DLL_SYM_COUNT_V101;
        break;
    
    default:
        sym_count = DIVA_DLL_SYM_COUNT_V100;
        break;
    }
    hr = dll_bind(&diva_dll, src, &sym, sym_count);

    if (FAILED(hr)) {
        if (src != self) {
            dprintf("Diva IO: Custom IO DLL does not provide function "
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
