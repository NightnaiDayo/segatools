#pragma once

#include <stddef.h>

#include "hooklib/dvd.h"
#include "hooklib/touch.h"
#include "hooklib/printer_chc.h"
#include "hooklib/printer_cx.h"

struct y3ws_config {
    bool enable;
    bool debug;

    uint16_t port;
    char game_id[5];
    wchar_t card_path[MAX_PATH];
};

void y3ws_config_load(struct y3ws_config *cfg, const wchar_t *filename);