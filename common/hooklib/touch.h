#pragma once

#include <windows.h>

#include <stdbool.h>

struct touch_screen_config {
    bool enable;
    bool remap;
    bool cursor;
};

/* Init is not thread safe because API hook init is not thread safe blah
    blah blah you know the drill by now. */

void touch_screen_hook_init(const struct touch_screen_config *cfg, HINSTANCE self);
void touch_hook_insert_hooks(HMODULE target);
