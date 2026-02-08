#include <windows.h>

#include <assert.h>
#include <stdlib.h>

#include "board/aime-dll.h"

#include "util/dll-bind.h"
#include "util/dprintf.h"

enum {
    AIME_DLL_SYM_COUNT_V100 = 5,
    AIME_DLL_SYM_COUNT_V101 = 17,
};

const struct dll_bind_sym aime_dll_syms[] = {
    {
        .sym = "aime_io_init",
        .off = offsetof(struct aime_dll, init),
    }, {
        .sym = "aime_io_nfc_poll",
        .off = offsetof(struct aime_dll, nfc_poll),
    }, {
        .sym = "aime_io_nfc_get_aime_id",
        .off = offsetof(struct aime_dll, nfc_get_aime_id),
    }, {
        .sym = "aime_io_nfc_get_felica_id",
        .off = offsetof(struct aime_dll, nfc_get_felica_id),
    }, {
        .sym = "aime_io_led_set_color",
        .off = offsetof(struct aime_dll, led_set_color),
    }, {
        .sym = "aime_io_vfd_set_text",
        .off = offsetof(struct aime_dll, vfd_set_text),
    }, {
        .sym = "aime_io_vfd_set_state",
        .off = offsetof(struct aime_dll, vfd_set_state),
    }, {
        .sym = "aime_io_nfc_get_mifare_uid",
        .off = offsetof(struct aime_dll, nfc_get_mifare_uid),
    }, {
        .sym = "aime_io_nfc_mifare_select",
        .off = offsetof(struct aime_dll, nfc_mifare_select),
    }, {
        .sym = "aime_io_nfc_mifare_set_key",
        .off = offsetof(struct aime_dll, nfc_mifare_set_key),
    }, {
        .sym = "aime_io_nfc_mifare_authenticate",
        .off = offsetof(struct aime_dll, nfc_mifare_authenticate),
    }, {
        .sym = "aime_io_nfc_mifare_read_block",
        .off = offsetof(struct aime_dll, nfc_mifare_read_block),
    }, {
        .sym = "aime_io_nfc_felica_transact",
        .off = offsetof(struct aime_dll, nfc_felica_transact),
    }, {
        .sym = "aime_io_nfc_radio_on",
        .off = offsetof(struct aime_dll, nfc_radio_on),
    }, {
        .sym = "aime_io_nfc_radio_off",
        .off = offsetof(struct aime_dll, nfc_radio_off),
    }, {
        .sym = "aime_io_nfc_to_update_mode",
        .off = offsetof(struct aime_dll, nfc_to_update_mode),
    }, {
        .sym = "aime_io_nfc_send_hex_data",
        .off = offsetof(struct aime_dll, nfc_send_hex_data),
    },
};

struct aime_dll aime_dll;

// Copypasta DLL binding and diagnostic message boilerplate.
// Not much of this lends itself to being easily factored out. Also there
// will be a lot of API-specific branching code here eventually as new API
// versions get defined, so even though these functions all look the same
// now this won't remain the case forever.

HRESULT aime_dll_init(const struct aime_dll_config *cfg, HINSTANCE self)
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
            dprintf("NFC Assembly: Failed to load IO DLL: %lx: %S\n",
                    hr,
                    cfg->path);

            goto end;
        }

        dprintf("NFC Assembly: Using custom IO DLL: %S\n", cfg->path);
        src = owned;
    } else {
        owned = NULL;
        src = self;
    }

    get_api_version = (void *) GetProcAddress(src, "aime_io_get_api_version");

    if (get_api_version != NULL) {
        aime_dll.api_version = get_api_version();
    } else {
        aime_dll.api_version = 0x0100;
        dprintf("Custom IO DLL does not expose aime_io_get_api_version, "
                "assuming API version 1.0.\n"
                "Please ask the developer to update their DLL.\n");
    }

    if (aime_dll.api_version >= 0x0200) {
        hr = E_NOTIMPL;
        dprintf("NFC Assembly: Custom IO DLL implements an unsupported "
                "API version (%#04x). Please update Segatools.\n",
                aime_dll.api_version);

        goto end;
    }

    sym = aime_dll_syms;
    sym_count = (aime_dll.api_version < 0x0101)
            ? AIME_DLL_SYM_COUNT_V100
            : AIME_DLL_SYM_COUNT_V101;
    hr = dll_bind(&aime_dll, src, &sym, sym_count);

    if (FAILED(hr)) {
        if (src != self) {
            dprintf("NFC Assembly: Custom IO DLL does not provide function "
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
