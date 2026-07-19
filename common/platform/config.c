#include <windows.h>
#include <winsock2.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void epay_config_load(struct epay_config *cfg, const wchar_t *filename)
{
    assert(cfg != NULL);
    assert(filename != NULL);

    cfg->enable = GetPrivateProfileIntW(L"epay", L"enable", 1, filename);
    cfg->hook = GetPrivateProfileIntW(L"epay", L"hook", 1, filename);
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
