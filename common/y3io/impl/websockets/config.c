#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>

#include "config.h"

void y3ws_config_load(struct y3ws_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"y3ws", L"enable", 1, filename);
    cfg->debug = GetPrivateProfileIntW(L"y3ws", L"debug", 0, filename);

    cfg->port = GetPrivateProfileIntW(L"y3ws", L"port", 3594, filename);

    wchar_t tmpstr[16];

    GetPrivateProfileStringW(
            L"y3ws",
            L"gameId",
            L"SDEY",
            tmpstr,
            _countof(tmpstr),
            filename);

    wcstombs(cfg->game_id, tmpstr, sizeof(cfg->game_id));

    GetPrivateProfileStringW(
            L"y3ws",
            L"cardDirectory",
            L"DEVICE\\print",
            cfg->card_path,
            _countof(cfg->card_path),
            filename);
}
