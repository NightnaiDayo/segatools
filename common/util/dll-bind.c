#include <windows.h>

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#include "util/dll-bind.h"
#include "util/dprintf.h"

HRESULT dll_bind(
    void *dest,
    HINSTANCE src,
    const struct dll_bind_sym **syms_pos,
    size_t syms_count,
    bool optional)
{
    HRESULT hr;
    void *src_result;
    void **dest_field;
    const struct dll_bind_sym *current_sym;
    size_t i;

    assert(dest != NULL);
    assert(src != NULL);
    assert(syms_pos != NULL);

    hr = S_OK;
    current_sym = *syms_pos;

    assert(current_sym != NULL);

    for (i = 0; i < syms_count; i++) {
        src_result = GetProcAddress(src, current_sym->sym);

        if (src_result != NULL) {
            dest_field = (void **)(((uint8_t *)dest) + current_sym->off);
            *dest_field = src_result;
        } else if (!optional) {
            hr = E_NOTIMPL;
            break;
        } else {
            dprintf("DLL Binding: Custom IO DLL does not provide function "
                    "\"%s\". The default implementation will be used.\n",
                    current_sym->sym);
        }

        current_sym++;
    }

    *syms_pos = current_sym;

    return hr;
}
