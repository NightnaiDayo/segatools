#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "apm3io/backend.h"
#include "apm3io/config.h"
#include "apm3io/apm3io.h"
#include "apm3io/kb.h"
#include "apm3io/di.h"
#include "apm3io/xi.h"

#include "util/dprintf.h"
#include "util/str.h"

static struct apm3_io_config apm3_io_cfg;
static const struct apm3_io_backend *apm3_io_backend;
static bool apm3_io_coin;

uint16_t apm3_io_get_api_version(void)
{
    return 0x0100;
}

HRESULT apm3_io_init(void)
{
    HINSTANCE inst;
    HRESULT hr;

    assert(apm3_io_backend == NULL);

    inst = GetModuleHandleW(NULL);

    if (inst == NULL) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        dprintf("GetModuleHandleW failed: %lx\n", hr);

        return hr;
    }

    apm3_io_config_load(&apm3_io_cfg, L".\\segatools.ini");

    if (wstr_ieq(apm3_io_cfg.mode, L"keyboard")) {
        hr = apm3_kb_init(&apm3_io_cfg.kb, &apm3_io_backend);
    } else if (wstr_ieq(apm3_io_cfg.mode, L"dinput")) {
        hr = apm3_di_init(&apm3_io_cfg.di, inst, &apm3_io_backend);
    } else if (wstr_ieq(apm3_io_cfg.mode, L"xinput")) {
        hr = apm3_xi_init(&apm3_io_cfg.xi, &apm3_io_backend);
    } else {
        hr = E_INVALIDARG;
        dprintf("APM3 IO: Invalid IO mode \"%S\", use keyboard, dinput or xinput\n",
                apm3_io_cfg.mode);
    }

    return hr;
}

void apm3_io_get_opbtns(uint8_t *opbtn_out)
{
    uint8_t opbtn;

    assert(apm3_io_backend != NULL);
    assert(opbtn_out != NULL);

    opbtn = 0;

    if (GetAsyncKeyState(apm3_io_cfg.vk_test) & 0x8000) {
        opbtn |= APM3_IO_OPBTN_TEST;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_service) & 0x8000) {
        opbtn |= APM3_IO_OPBTN_SERVICE;
    }

    if (GetAsyncKeyState(apm3_io_cfg.vk_coin) & 0x8000) {
        if (!apm3_io_coin) {
            apm3_io_coin = true;
            opbtn |= APM3_IO_OPBTN_COIN;
        }
    } else {
        apm3_io_coin = false;
    }

    *opbtn_out = opbtn;
}


void apm3_io_get_gamebtns(uint16_t *gamebtn_out)
{
    assert(apm3_io_backend != NULL);
    assert(gamebtn_out != NULL);

    apm3_io_backend->get_gamebtns(gamebtn_out);
}

HRESULT apm3_io_led_init(void)
{
    return S_OK;
}

void apm3_io_led_set_colors(uint8_t board, uint8_t *rgb)
{
    return;
}
