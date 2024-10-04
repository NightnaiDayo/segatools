#include <windows.h>

#include <assert.h>
#include <stdlib.h>

#include "kemonohook/kemono-dll.h"

#include "util/dll-bind.h"
#include "util/dprintf.h"

const struct dll_bind_sym kemono_dll_syms[] = {
        {
                .sym = "kemono_io_init",
                .off = offsetof(struct kemono_dll, init),
        },
        {
                .sym = "kemono_io_poll",
                .off = offsetof(struct kemono_dll, poll),
        },
        {
                .sym = "kemono_io_jvs_read_coin_counter",
                .off = offsetof(struct kemono_dll, jvs_read_coin_counter),
        },
        {
                .sym = "kemono_io_led_init",
                .off = offsetof(struct kemono_dll, led_init),
        },
        {
                .sym = "kemono_io_led_set_colors",
                .off = offsetof(struct kemono_dll, led_set_leds),
        },
        {
                .sym = "kemono_io_jvs_write_gpio",
                .off = offsetof(struct kemono_dll, jvs_write_gpio),
        }
};

struct kemono_dll kemono_dll;

// Copypasta DLL binding and diagnostic message boilerplate.
// Not much of this lends itself to being easily factored out. Also there
// will be a lot of API-specific branching code here eventually as new API
// versions get defined, so even though these functions all look the same
// now this won't remain the case forever.

HRESULT kemono_dll_init(const struct kemono_dll_config *cfg, HINSTANCE self) {
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
            dprintf("Kemono IO: Failed to load IO DLL: %lx: %S\n",
                    hr,
                    cfg->path);

            goto end;
        }

        dprintf("Kemono IO: Using custom IO DLL: %S\n", cfg->path);
        src = owned;
    } else {
        owned = NULL;
        src = self;
    }

    get_api_version = (void *) GetProcAddress(src, "kemono_io_get_api_version");

    if (get_api_version != NULL) {
        kemono_dll.api_version = get_api_version();
    } else {
        kemono_dll.api_version = 0x0100;
        dprintf("Custom IO DLL does not expose kemono_io_get_api_version, "
                "assuming API version 1.0.\n"
                "Please ask the developer to update their DLL.\n");
    }

    if (kemono_dll.api_version >= 0x0200) {
        hr = E_NOTIMPL;
        dprintf("Kemono IO: Custom IO DLL implements an unsupported "
                "API version (%#04x). Please update Segatools.\n",
                kemono_dll.api_version);

        goto end;
    }

    sym = kemono_dll_syms;
    hr = dll_bind(&kemono_dll, src, &sym, _countof(kemono_dll_syms));

    if (FAILED(hr)) {
        if (src != self) {
            dprintf("Kemono IO: Custom IO DLL does not provide function "
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
