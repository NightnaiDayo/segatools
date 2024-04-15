#include "config.h"

void unity_config_load(struct unity_config *cfg, const wchar_t *filename) {
    cfg->enable = GetPrivateProfileIntW(L"unity", L"enable", 1, filename);

    GetPrivateProfileStringW(
        L"unity",
        L"targetAssembly",
        L"",
        cfg->target_assembly,
        _countof(cfg->target_assembly),
        filename
    );
}
