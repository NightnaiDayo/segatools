#include <windows.h>

#include <assert.h>
#include <stdlib.h>

#include "apm3hook/apm-dll.h"

#include "util/dll-bind.h"
#include "util/dprintf.h"

const struct dll_bind_sym apm_dll_syms[] = {
    {
        .sym = "apm_io_init",
        .off = offsetof(struct apm_dll, init),
    }, {
        .sym = "apm_io_poll",
        .off = offsetof(struct apm_dll, poll),
    }, {
        .sym = "apm_io_get_opbtns",
        .off = offsetof(struct apm_dll, get_opbtns),
    }, {
        .sym = "apm_io_get_gamebtns",
        .off = offsetof(struct apm_dll, get_gamebtns),
    }, {
        .sym = "apm_io_led_init",
        .off = offsetof(struct apm_dll, led_init),
    }, {
        .sym = "apm_io_led_set_colors",
        .off = offsetof(struct apm_dll, led_set_leds),
    }
};

struct apm_dll apm_dll;

// Copypasta DLL binding and diagnostic message boilerplate.
// Not much of this lends itself to being easily factored out. Also there
// will be a lot of API-specific branching code here eventually as new API
// versions get defined, so even though these functions all look the same
// now this won't remain the case forever.

HRESULT apm_dll_init(const struct apm_dll_config *cfg, HINSTANCE self)
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
            dprintf("APM IO: Failed to load IO DLL: %lx: %S\n",
                    hr,
                    cfg->path);

            goto end;
        }

        dprintf("APM IO: Using custom IO DLL: %S\n", cfg->path);
        src = owned;
    } else {
        owned = NULL;
        src = self;
    }

    get_api_version = (void *) GetProcAddress(src, "apm_io_get_api_version");

    if (get_api_version != NULL) {
        apm_dll.api_version = get_api_version();
    } else {
        apm_dll.api_version = 0x0100;
        dprintf("Custom IO DLL does not expose apm_io_get_api_version, "
                "assuming API version 1.0.\n"
                "Please ask the developer to update their DLL.\n");
    }

    if (apm_dll.api_version >= 0x0200) {
        hr = E_NOTIMPL;
        dprintf("APM IO: Custom IO DLL implements an unsupported "
                "API version (%#04x). Please update Segatools.\n",
                apm_dll.api_version);

        goto end;
    }

    sym = apm_dll_syms;
    hr = dll_bind(&apm_dll, src, &sym, _countof(apm_dll_syms));

    if (FAILED(hr)) {
        if (src != self) {
            dprintf("APM IO: Custom IO DLL does not provide function "
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
