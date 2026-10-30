#include <windows.h>
#include <winsock2.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "platform/amvideo.h"
#include "platform/clock.h"
#include "platform/config.h"

#include <shlwapi.h>

#include "platform/dns.h"
#include "platform/epay.h"
#include "platform/hwmon.h"
#include "platform/hwreset.h"
#include "platform/misc.h"
#include "platform/netenv.h"
#include "platform/nusec.h"
#include "platform/pcbid.h"
#include "platform/registry.h"
#include "platform/platform.h"
#include "platform/vfs.h"
#include "platform/system.h"
#include "util/dprintf.h"
#include "platform/openssl.h"
#include "util/dprintf.h"


void platform_config_load(struct platform_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    if (!PathFileExistsW(filename)) {
        wchar_t temp[MAX_PATH];
        dprintf("ERROR: Configuration does not exist\n");
        dprintf("            Configured: \"%ls\"\n", filename);
        GetFullPathNameW(filename, _countof(temp), temp, NULL);
        dprintf("            Expanded:   \"%ls\"\n", temp);
    }

    amvideo_config_load(&cfg->amvideo, filename);
    clock_config_load(&cfg->clock, filename);
    dns_config_load(&cfg->dns, filename);
    epay_config_load(&cfg->epay, filename);
    hwmon_config_load(&cfg->hwmon, filename);
    hwreset_config_load(&cfg->hwreset, filename);
    misc_config_load(&cfg->misc, filename);
    pcbid_config_load(&cfg->pcbid, filename);
    registry_config_load(&cfg->registry, filename);
    netenv_config_load(&cfg->netenv, filename);
    nusec_config_load(&cfg->nusec, filename);
    vfs_config_load(&cfg->vfs, filename);
    system_config_load(&cfg->system, filename);
    openssl_config_load(&cfg->openssl, filename);
    ewf_config_load(&cfg->ewf, filename);
}

void amvideo_config_load(struct amvideo_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"amvideo", L"enable", 1, filename);
}

void clock_config_load(struct clock_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->timezone = GetPrivateProfileIntW(L"clock", L"timezone", 1, filename);
    cfg->timewarp = GetPrivateProfileIntW(L"clock", L"timewarp", 0, filename);
    cfg->writeable = GetPrivateProfileIntW(
            L"clock",
            L"writeable",
            0,
            filename);
}

void dns_config_load(struct dns_config *cfg, const wchar_t *filename)
{
    wchar_t default_[128];

    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"dns", L"enable", 1, filename);

    GetPrivateProfileStringW(
            L"dns",
            L"default",
            L"localhost",
            default_,
            _countof(default_),
            filename);

    GetPrivateProfileStringW(
            L"dns",
            L"router",
            default_,
            cfg->router,
            _countof(cfg->router),
            filename);

    GetPrivateProfileStringW(
            L"dns",
            L"startup",
            default_,
            cfg->startup,
            _countof(cfg->startup),
            filename);

    GetPrivateProfileStringW(
            L"dns",
            L"billing",
            default_,
            cfg->billing,
            _countof(cfg->billing),
            filename);

    GetPrivateProfileStringW(
            L"dns",
            L"aimedb",
            default_,
            cfg->aimedb,
            _countof(cfg->aimedb),
            filename);

    GetPrivateProfileStringW(
            L"dns",
            L"title",
            L"title",
            cfg->title,
            _countof(cfg->title),
            filename);

    cfg->replaceHost = GetPrivateProfileIntW(L"dns", L"replaceHost", 0, filename);

    cfg->startupPort = GetPrivateProfileIntW(L"dns", L"startupPort", 0, filename);
    cfg->billingPort = GetPrivateProfileIntW(L"dns", L"billingPort", 0, filename);
    cfg->aimedbPort = GetPrivateProfileIntW(L"dns", L"aimedbPort", 0, filename);
}

void hwmon_config_load(struct hwmon_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"hwmon", L"enable", 1, filename);
}

void hwreset_config_load(struct hwreset_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"hwreset", L"enable", 1, filename);
}

void misc_config_load(struct misc_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"misc", L"enable", 1, filename);
    cfg->allowMasterKeyWrite = GetPrivateProfileIntW(L"misc", L"allowMasterKeyWrite", 0, filename);
    cfg->allowReboot = GetPrivateProfileIntW(L"misc", L"allowReboot", 0, filename);
    GetPrivateProfileStringW(
            L"misc",
            L"nextProcessFilePath",
            L"DEVICE\\NextProcess.txt",
            cfg->nextProcessFile,
            _countof(cfg->nextProcessFile),
            filename);
}

void netenv_config_load(struct netenv_config *cfg, const wchar_t *filename)
{
    wchar_t mac_addr[18];

    assert(cfg != NULL);
    assert(filename != NULL);

    memset(cfg, 0, sizeof(*cfg));

    cfg->enable = GetPrivateProfileIntW(L"netenv", L"enable", 0, filename);
    cfg->redirect_broadcast = GetPrivateProfileIntW(
            L"netenv",
            L"redirectBroadcast",
            1,
            filename);

    cfg->addr_suffix = GetPrivateProfileIntW(
            L"netenv",
            L"addrSuffix",
            11,
            filename);

    cfg->router_suffix = GetPrivateProfileIntW(
            L"netenv",
            L"routerSuffix",
            254,
            filename);

    GetPrivateProfileStringW(
            L"netenv",
            L"macAddr",
            L"01:02:03:04:05:06",
            mac_addr,
            _countof(mac_addr),
            filename);

    swscanf(mac_addr,
            L"%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
            &cfg->mac_addr[0],
            &cfg->mac_addr[1],
            &cfg->mac_addr[2],
            &cfg->mac_addr[3],
            &cfg->mac_addr[4],
            &cfg->mac_addr[5],
            &cfg->mac_addr[6]);
}

void nusec_config_load(struct nusec_config *cfg, const wchar_t *filename)
{
    wchar_t keychip_id[17];
    wchar_t game_id[5];
    wchar_t platform_id[5];
    wchar_t subnet[16];
    wchar_t bcast[16];
    unsigned int ip[4];
    size_t i;

    assert(cfg != NULL);
    assert(filename != NULL);

    memset(cfg, 0, sizeof(*cfg));
    memset(keychip_id, 0, sizeof(keychip_id));
    memset(game_id, 0, sizeof(game_id));
    memset(platform_id, 0, sizeof(platform_id));
    memset(subnet, 0, sizeof(subnet));
    memset(bcast, 0, sizeof(bcast));

    cfg->enable = GetPrivateProfileIntW(L"keychip", L"enable", 1, filename);

    GetPrivateProfileStringW(
            L"keychip",
            L"id",
            L"A69E-01A88888888",
            keychip_id,
            _countof(keychip_id),
            filename);

    GetPrivateProfileStringW(
            L"keychip",
            L"gameId",
            L"",
            game_id,
            _countof(game_id),
            filename);

    GetPrivateProfileStringW(
            L"keychip",
            L"platformId",
            L"",
            platform_id,
            _countof(platform_id),
            filename);

    cfg->region = GetPrivateProfileIntW(L"keychip", L"region", 1, filename);
    cfg->billing_type = GetPrivateProfileIntW(L"keychip", L"billingType", 1, filename);
    cfg->system_flag = GetPrivateProfileIntW(
            L"keychip",
            L"systemFlag",
            0x64,
            filename);

    GetPrivateProfileStringW(
            L"keychip",
            L"subnet",
            L"192.168.100.0",
            subnet,
            _countof(subnet),
            filename);

    GetPrivateProfileStringW(
            L"netenv",
            L"broadcast",
            L"255.255.255.255",
            bcast,
            _countof(bcast),
            filename);

    for (i = 0 ; i < 16 ; i++) {
        cfg->keychip_id[i] = (char) keychip_id[i];
    }

    for (i = 0 ; i < 4 ; i++) {
        cfg->game_id[i] = (char) game_id[i];
    }

    for (i = 0 ; i < 4 ; i++) {
        cfg->platform_id[i] = (char) platform_id[i];
    }

    swscanf(subnet, L"%u.%u.%u.%u", &ip[0], &ip[1], &ip[2], &ip[3]);
    cfg->subnet = (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | 0;

    swscanf(bcast, L"%u.%u.%u.%u", &ip[0], &ip[1], &ip[2], &ip[3]);
    cfg->bcast = (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | (ip[3]);

    GetPrivateProfileStringW(
            L"keychip",
            L"billingCa",
            L"DEVICE\\ca.crt",
            cfg->billing_ca,
            _countof(cfg->billing_ca),
            filename);

    GetPrivateProfileStringW(
            L"keychip",
            L"billingPub",
            L"DEVICE\\billing.pub",
            cfg->billing_pub,
            _countof(cfg->billing_pub),
            filename);

    cfg->persistence = GetPrivateProfileIntW(L"keychip", L"persistence", 0, filename);

    GetPrivateProfileStringW(
            L"keychip",
            L"persistent_path",
            L"DEVICE\\nusec.bin",
            cfg->persistent_path,
            _countof(cfg->persistent_path),
            filename);
}

void pcbid_config_load(struct pcbid_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"pcbid", L"enable", 1, filename);

    GetPrivateProfileStringW(
            L"pcbid",
            L"serialNo",
            L"ACAE01A99999999",
            cfg->serial_no,
            _countof(cfg->serial_no),
            filename);
}

void vfs_config_load(struct vfs_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"vfs", L"enable", 1, filename);
    cfg->allowAmfsDownloads = GetPrivateProfileIntW(L"vfs", L"allowAmfsDownloads", 0, filename);

    GetPrivateProfileStringW(
            L"vfs",
            L"amfs",
            L"",
            cfg->amfs,
            _countof(cfg->amfs),
            filename);

    GetPrivateProfileStringW(
            L"vfs",
            L"appdata",
            L"",
            cfg->appdata,
            _countof(cfg->appdata),
            filename);

    GetPrivateProfileStringW(
            L"vfs",
            L"option",
            L"",
            cfg->option,
            _countof(cfg->option),
            filename);

    for (int i = 0; i < MAX_REDIRECTIONS; i++){
        wchar_t key[32];
        wsprintfW(key, L"redirection%dfrom", i);
        GetPrivateProfileStringW(
                    L"vfs",
                    key,
                    L"",
                    cfg->redirections_from[i],
                    _countof(cfg->redirections_from[i]),
                    filename);
        wsprintfW(key, L"redirection%dto", i);
        GetPrivateProfileStringW(
                    L"vfs",
                    key,
                    L"",
                    cfg->redirections_to[i],
                    _countof(cfg->redirections_to[i]),
                    filename);

        cfg->redirections_from_len[i] = (int)wcslen(cfg->redirections_from[i]);

        if (cfg->redirections_from_len[i] > 0) {
            dprintf("Vfs: Set up custom redirection from %ls to %ls\n", cfg->redirections_from[i], cfg->redirections_to[i]);
        }
    }
}

void system_config_load(struct system_config *cfg, const wchar_t *filename)
{
    wchar_t name[7];
    size_t i;

    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"system", L"enable", 0, filename);
    cfg->freeplay = GetPrivateProfileIntW(L"system", L"freeplay", 0, filename);

    wcscpy_s(name, _countof(name), L"dipsw0");

    for (i = 0 ; i < 8 ; i++) {
        name[5] = L'1' + i;
        cfg->dipsw[i] = GetPrivateProfileIntW(L"system", name, 0, filename);
    }
}

static bool registry_config_path_is_drive_absolute(const wchar_t *path)
{
    assert(path != NULL);

    return
            ((path[0] >= L'A' && path[0] <= L'Z') ||
             (path[0] >= L'a' && path[0] <= L'z')) &&
            path[1] == L':' &&
            path[2] == L'\\';
}

static bool registry_config_path_is_remote_drive(const wchar_t *path)
{
    wchar_t root[] = L"C:\\";

    assert(path != NULL);
    assert(registry_config_path_is_drive_absolute(path));

    root[0] = path[0];

    return GetDriveTypeW(root) == DRIVE_REMOTE;
}

static bool registry_config_component_equals(
        const wchar_t *component,
        size_t ncomponent,
        const wchar_t *expected)
{
    return wcslen(expected) == ncomponent &&
            _wcsnicmp(component, expected, ncomponent) == 0;
}

static bool registry_config_path_has_unsupported_component(
        const wchar_t *path)
{
    const wchar_t *base_end;
    const wchar_t *component;
    const wchar_t *cursor;
    size_t base_len;

    assert(path != NULL);

    component = path + 3;

    while (*component != L'\0') {
        cursor = component;

        while (*cursor != L'\0' && *cursor != L'\\' && *cursor != L'/') {
            if (*cursor < L' ' || wcschr(L"\"<>|?*:", *cursor) != NULL) {
                return true;
            }

            cursor++;
        }

        if (cursor == component || cursor[-1] == L' ' || cursor[-1] == L'.') {
            return true;
        }

        base_end = component;

        while (base_end < cursor && *base_end != L'.') {
            base_end++;
        }

        base_len = (size_t) (base_end - component);

        if (registry_config_component_equals(component, base_len, L"CON") ||
                registry_config_component_equals(component, base_len, L"PRN") ||
                registry_config_component_equals(component, base_len, L"AUX") ||
                registry_config_component_equals(component, base_len, L"NUL") ||
                registry_config_component_equals(component, base_len, L"CLOCK$") ||
                registry_config_component_equals(component, base_len, L"CONIN$") ||
                registry_config_component_equals(component, base_len, L"CONOUT$") ||
                (base_len == 4 &&
                 (_wcsnicmp(component, L"COM", 3) == 0 ||
                  _wcsnicmp(component, L"LPT", 3) == 0) &&
                 ((component[3] >= L'1' && component[3] <= L'9') ||
                  component[3] == L'\u00B9' ||
                  component[3] == L'\u00B2' ||
                  component[3] == L'\u00B3'))) {
            return true;
        }

        component = *cursor == L'\0' ? cursor : cursor + 1;
    }

    return false;
}

static enum registry_path_error registry_config_resolve_path(
        const wchar_t *filename,
        const wchar_t *path,
        wchar_t *resolved,
        size_t nresolved)
{
    wchar_t config_dir[MAX_PATH];
    wchar_t combined[MAX_PATH];
    const wchar_t *full;
    DWORD count;

    assert(filename != NULL);
    assert(path != NULL);
    assert(resolved != NULL);

    resolved[0] = L'\0';

    if (PathIsRelativeW(path)) {
        count = GetFullPathNameW(
                filename,
                _countof(config_dir),
                config_dir,
                NULL);

        if (count == 0 || count >= _countof(config_dir)) {
            return REGISTRY_PATH_TOO_LONG;
        }

        PathRemoveFileSpecW(config_dir);

        if (PathCombineW(combined, config_dir, path) == NULL) {
            return REGISTRY_PATH_TOO_LONG;
        }

        full = combined;
    } else {
        full = path;
    }

    if (!registry_config_path_is_drive_absolute(full)) {
        wcscpy_s(resolved, nresolved, full);

        return REGISTRY_PATH_NOT_LOCAL;
    }

    count = GetFullPathNameW(full, (DWORD) nresolved, resolved, NULL);

    if (count >= nresolved) {
        resolved[0] = L'\0';

        return REGISTRY_PATH_TOO_LONG;
    }

    if (count == 0 || !registry_config_path_is_drive_absolute(resolved)) {
        wcscpy_s(resolved, nresolved, full);

        return REGISTRY_PATH_BAD_NAME;
    }

    if (registry_config_path_is_remote_drive(resolved)) {
        return REGISTRY_PATH_NOT_LOCAL;
    }

    if (registry_config_path_has_unsupported_component(resolved)) {
        return REGISTRY_PATH_BAD_NAME;
    }

    return REGISTRY_PATH_OK;
}

void registry_config_load(
        struct registry_config *cfg,
        const wchar_t *filename)
{
    wchar_t path[MAX_PATH + 1];
    wchar_t profile[REGISTRY_STORE_PROFILE_MAX + 2];
    DWORD count;
    size_t i;

    assert(cfg != NULL);
    assert(filename != NULL);

    memset(cfg, 0, sizeof(*cfg));

    count = GetPrivateProfileStringW(
            L"reg",
            L"path",
            L"",
            path,
            _countof(path),
            filename);

    if (path[0] == L'\0') {
        wcscpy_s(path, _countof(path), L"DEVICE\\reg\\segatools.hiv");
    } else if (count >= MAX_PATH) {
        cfg->path_error = REGISTRY_PATH_TOO_LONG;
    }

    if (cfg->path_error == REGISTRY_PATH_OK) {
        for (i = 0; path[i] != L'\0'; i++) {
            if (path[i] == L'/') {
                path[i] = L'\\';
            }
        }

        cfg->path_error = registry_config_resolve_path(
                filename,
                path,
                cfg->path,
                _countof(cfg->path));
    }

    count = GetPrivateProfileStringW(
            L"reg",
            L"profile",
            L"",
            profile,
            _countof(profile),
            filename);

    if (count > REGISTRY_STORE_PROFILE_MAX) {
        cfg->profile_truncated = true;
    } else {
        wcscpy_s(cfg->profile, _countof(cfg->profile), profile);
    }
}

void epay_config_load(struct epay_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"epay", L"enable", 1, filename);
    cfg->hook = GetPrivateProfileIntW(L"epay", L"hook", 1, filename);
    cfg->reg_redirect = (int) GetPrivateProfileIntW(
            L"epay",
            L"regRedirect",
            0,
            filename);
}

void openssl_config_load(struct openssl_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"openssl", L"enable", 1, filename);
    cfg->override = GetPrivateProfileIntW(L"openssl", L"override", 0, filename);
}

void ewf_config_load(struct ewf_config* cfg, const wchar_t* filename) {
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"ewf", L"enable", 0, filename);
    cfg->full = GetPrivateProfileIntW(L"ewf", L"full", 0, filename);
}
