/*
 * Hook for an ominous touch panel dll. Works via WinTouch by default, falls back to a SHARP touch panel on port 1 otherwise (which is taken by the VFD already), however all this dll does is memcpy'ing some data around and has no references to WinTouch functions, and doesn't start any threads, so the relevant call get stubbed here. /shrug
 */

#include "amptw.h"

#include <assert.h>
#include <stdlib.h>

#include "hook/procaddr.h"
#include "hook/table.h"
#include "hooklib/touch.h"
#include "util/dprintf.h"

static int hook_Amptw_getResult(void* pContext, int* pResult);

static const struct hook_symbol touch_hooks[] = {
    {
        .name  = "Amptw_getResult",
        .patch = hook_Amptw_getResult
    },
};

void amptw_hook_init(const struct touch_screen_config* cfg, HMODULE hmodule) {
    assert(cfg != NULL);

    if (!cfg->enable) {
        return;
    }

    amptw_apply_hooks(hmodule);
}

void amptw_apply_hooks(HMODULE module) {
    hook_table_apply(module, "amptw.dll", touch_hooks, _countof(touch_hooks));

    proc_addr_table_push(module, "amptw.dll", touch_hooks,
                         _countof(touch_hooks));
}

static int hook_Amptw_getResult(void* pContext, int* pResult) {
    *pResult = 0;
    return 0;
}