#include <stdlib.h>

#include "config.h"

void unity_config_load(struct unity_config *cfg, const wchar_t *filename) {
    cfg->enable = GetPrivateProfileIntW(L"unity", L"enable", 1, filename);
    cfg->debug_enable = GetPrivateProfileIntW(L"unity", L"enableDebug", 0, filename);
    cfg->debug_suspend = GetPrivateProfileIntW(L"unity", L"debugSuspend", 0, filename);

    GetPrivateProfileStringW(
        L"unity",
        L"targetAssembly",
        L"",
        cfg->target_assembly,
        _countof(cfg->target_assembly),
        filename
    );
    
    GetPrivateProfileStringW(
        L"unity",
        L"debugAddress",
        L"127.0.0.1:55555",
        cfg->debug_address,
        _countof(cfg->debug_address),
        filename
    );
}
