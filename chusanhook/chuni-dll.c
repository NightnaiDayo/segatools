#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <stdlib.h>

#include "chuniio/chu2to3.h"
#include "chusanhook/chuni-dll.h"

#include "util/dll-bind.h"
#include "util/dprintf.h"

const struct dll_bind_sym chuni_dll_syms[] = {
    {
        .sym = "chuni_io_jvs_init",
        .off = offsetof(struct chuni_dll, jvs_init),
    }, {
        .sym = "chuni_io_jvs_poll",
        .off = offsetof(struct chuni_dll, jvs_poll),
    }, {
        .sym = "chuni_io_jvs_read_coin_counter",
        .off = offsetof(struct chuni_dll, jvs_read_coin_counter),
    }, {
        .sym = "chuni_io_slider_init",
        .off = offsetof(struct chuni_dll, slider_init),
    }, {
        .sym = "chuni_io_slider_start",
        .off = offsetof(struct chuni_dll, slider_start),
    }, {
        .sym = "chuni_io_slider_stop",
        .off = offsetof(struct chuni_dll, slider_stop),
    }, {
        .sym = "chuni_io_slider_set_leds",
        .off = offsetof(struct chuni_dll, slider_set_leds),
    }, {
        .sym = "chuni_io_led_init",
        .off = offsetof(struct chuni_dll, led_init),
    }, {
        .sym = "chuni_io_led_set_colors",
        .off = offsetof(struct chuni_dll, led_set_leds),
    }
};

const struct dll_bind_sym chu2to3_dll_syms[] = {
    {
        .sym = "chu2to3_io_jvs_init",
        .off = offsetof(struct chuni_dll, jvs_init),
    }, {
        .sym = "chu2to3_io_jvs_poll",
        .off = offsetof(struct chuni_dll, jvs_poll),
    }, {
        .sym = "chu2to3_io_jvs_read_coin_counter",
        .off = offsetof(struct chuni_dll, jvs_read_coin_counter),
    }, {
        .sym = "chu2to3_io_slider_init",
        .off = offsetof(struct chuni_dll, slider_init),
    }, {
        .sym = "chu2to3_io_slider_start",
        .off = offsetof(struct chuni_dll, slider_start),
    }, {
        .sym = "chu2to3_io_slider_stop",
        .off = offsetof(struct chuni_dll, slider_stop),
    }, {
        .sym = "chu2to3_io_slider_set_leds",
        .off = offsetof(struct chuni_dll, slider_set_leds),
    }, {
        .sym = "chu2to3_io_led_init",
        .off = offsetof(struct chuni_dll, led_init),
    }, {
        .sym = "chu2to3_io_led_set_colors",
        .off = offsetof(struct chuni_dll, led_set_leds),
    }
};

/* Helper function to determine upon dll_bind failure whether the required functions were found
   NOTE: relies on symbols order declared above */
static HRESULT has_enough_symbols(uint16_t version, uint8_t count)
{
    if ( version <= 0x0101 && count == 7 )
        return S_OK;

    if ( version >= 0x0102 && count == 9 )
        return S_OK;

    return E_FAIL;
}

struct chuni_dll chuni_dll;

// Copypasta DLL binding and diagnostic message boilerplate.
// Not much of this lends itself to being easily factored out. Also there
// will be a lot of API-specific branching code here eventually as new API
// versions get defined, so even though these functions all look the same
// now this won't remain the case forever.

HRESULT chuni_dll_init(const struct chuni_dll_config *cfg, HINSTANCE self)
{
    uint16_t (*get_api_version)(void);
    const struct dll_bind_sym *sym;
    HINSTANCE owned;
    HINSTANCE src;
    HRESULT hr;

    assert(cfg != NULL);
    assert(self != NULL);

    owned = NULL;
    src = self;

    if (cfg->chu2to3) {
        dprintf("Chunithm IO: using chu2to3 engine for IO DLL: %S\n", cfg->path);
	} else if (cfg->path[0] != L'\0') {
        owned = LoadLibraryW(cfg->path);

        if (owned == NULL) {
            hr = HRESULT_FROM_WIN32(GetLastError());
            dprintf("Chunithm IO: Failed to load IO DLL: %lx: %S\n",
                    hr,
                    cfg->path);

            goto end;
        }

        dprintf("Chunithm IO: Using custom IO DLL: %S\n", cfg->path);
        src = owned;
    }

    if (cfg->chu2to3) {
        if (chu2to3_load_dll(cfg->path) != 0)
		    dprintf("Could not init chu2to3 engine\n");

        get_api_version = (void *) GetProcAddress(src, "chu2to3_io_get_api_version");
    }
	else
	{
        get_api_version = (void *) GetProcAddress(src, "chuni_io_get_api_version");
	}

    if (get_api_version != NULL) {
        chuni_dll.api_version = get_api_version();
    } else {
        chuni_dll.api_version = 0x0100;
        dprintf("Custom IO DLL does not expose chuni_io_get_api_version, "
                "assuming API version 1.0.\n"
                "Please ask the developer to update their DLL.\n");
    }

    if (chuni_dll.api_version >= 0x0200) {
        hr = E_NOTIMPL;
        dprintf("Chunithm IO: Custom IO DLL implements an unsupported "
                "API version (%#04x). Please update Segatools.\n",
                chuni_dll.api_version);

        goto end;
    }

    sym = cfg->chu2to3 ? chu2to3_dll_syms : chuni_dll_syms;
    const struct dll_bind_sym *init_sym = &sym[0];

    hr = dll_bind(&chuni_dll, src, &sym, _countof(chuni_dll_syms));

    if (FAILED(hr)) {
        if (src != self) {
            // Might still be ok depending on external dll API version
            int bind_count = sym - init_sym;
            if ( has_enough_symbols(chuni_dll.api_version, bind_count) == S_OK )
            {
                hr = S_OK;
            } else {
                dprintf("Chunithm IO: Custom IO DLL does not provide function "
                        "\"%s\". Please contact your IO DLL's developer for "
                        "further assistance.\n",
                        sym->sym);
                dprintf("imported %d symbols\n", bind_count);
                goto end;
            }
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
