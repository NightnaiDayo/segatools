#include <windows.h>

#include <assert.h>

#include "hooklib/dns.h"

#include "platform/dns.h"

HRESULT dns_platform_hook_init(const struct dns_config *cfg)
{
    HRESULT hr;

    assert(cfg != NULL);

    if (!cfg->enable) {
        return S_FALSE;
    }

    if(cfg->replaceHost){
        http_hook_init();
    }

    hr = dns_hook_push(L"tenporouter.loc", cfg->router);

    if (FAILED(hr)) {
        return hr;
    }

    hr = dns_hook_push(L"bbrouter.loc", cfg->router);

    if (FAILED(hr)) {
        return hr;
    }

    hr = dns_hook_push(L"naominet.jp", cfg->startup);

    if (FAILED(hr)) {
        return hr;
    }

    hr = dns_hook_push(L"anbzvarg.wc", cfg->startup);

    if (FAILED(hr)) {
        return hr;
    }

    hr = dns_hook_push(L"op.auth.sys-all.net", cfg->startup);

    if (FAILED(hr)) {
        return hr;
    }

    hr = dns_hook_push(L"at.auth.sys-all.net", cfg->startup);

    if (FAILED(hr)) {
        return hr;
    }

    hr = dns_hook_push(L"at.sys-all.net", cfg->startup);

    if (FAILED(hr)) {
        return hr;
    }

    hr = dns_hook_push(L"ib.naominet.jp", cfg->billing);

    if (FAILED(hr)) {
        return hr;
    }

    hr = dns_hook_push(L"vo.anbzvarg.wc", cfg->billing);

    if (FAILED(hr)) {
        return hr;
    }

    hr = dns_hook_push(L"aime.naominet.jp", cfg->aimedb);

    if (FAILED(hr)) {
        return hr;
    }

    hr = dns_hook_push(L"nvzr.anbzvarg.wc", cfg->aimedb);

    if (FAILED(hr)) {
        return hr;
    }

    // crossbeats REV.
    hr = dns_hook_push(L"https://rev-ent.ac.capcom.jp:443", cfg->title);

    if (FAILED(hr)) {
        return hr;
    }

    // AimePay
    hr = dns_hook_push(L"api-aime.am-all.net", cfg->startup);

    if (FAILED(hr)) {
        return hr;
    }

    // E-MONEY
    hr = dns_hook_push(L"tasms-api-basis.thincacloud.com", cfg->startup);

    if (FAILED(hr)) {
        return hr;
    }

    hr = dns_hook_push(L"shop.tfps.thincacloud.com", cfg->startup);

    if (FAILED(hr)) {
        return hr;
    }
    
    // WAHLAP PowerOn
    hr = dns_hook_push(L"at.sys-all.cn", cfg->startup);

    if (FAILED(hr)) {
        return hr;
    }
    
    hr = dns_hook_push(L"at.sys-allnet.cn", cfg->startup);

    if (FAILED(hr)) {
        return hr;
    }
    
    // WAHLAP WeChat AimeDB Server
    hr = dns_hook_push(L"ai.sys-all.cn", cfg->aimedb);

    if (FAILED(hr)) {
        return hr;
    }
    
    hr = dns_hook_push(L"ai.sys-allnet.cn", cfg->aimedb);

    if (FAILED(hr)) {
        return hr;
    }
    
    // WAHLAP Billing
    hr = dns_hook_push(L"bl.sys-all.cn", cfg->billing);

    if (FAILED(hr)) {
        return hr;
    }
    
    hr = dns_hook_push(L"bl.sys-allnet.cn", cfg->billing);

    if (FAILED(hr)) {
        return hr;
    }

    // if your ISP resolves bad domains, it will kill the network. These 2
    // *cannot* resolve

    hr = dns_hook_push(L"mobirouter.loc", NULL);

    if (FAILED(hr)) {
        return hr;
    }

    hr = dns_hook_push(L"dslrouter.loc", NULL);

    if (FAILED(hr)) {
        return hr;
    }

    // Disable api/polling to the original servers

    hr = dns_hook_push(L"*.amlog.sys-all.net", NULL);

    if (FAILED(hr)) {
        return hr;
    }

    hr = dns_hook_push(L"*.d-amlog.sys-all.net", NULL);

    if (FAILED(hr)) {
        return hr;
    }

    return S_OK;
}
