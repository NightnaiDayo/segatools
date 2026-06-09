#include <windows.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hook/table.h"

#include "hooklib/dns.h"
#include "hooklib/reg.h"

#include "platform/epay.h"

#include <hooklib/path.h>

#include "util/dprintf.h"

static HRESULT misc_read_thinca_adapter(void *bytes, uint32_t *nbytes);
static HRESULT misc_read_ca_loc(void *bytes, uint32_t *nbytes);
static HRESULT misc_read_ca_client_loc(void *bytes, uint32_t *nbytes);
static HRESULT misc_read_network_timeout(void *bytes, uint32_t *nbytes);
static HRESULT misc_read_pattern0(void *bytes, uint32_t *nbytes);
static HRESULT misc_read_network_timeout0(void *bytes, uint32_t *nbytes);
static HRESULT misc_read_pattern1(void *bytes, uint32_t *nbytes);
static HRESULT misc_read_network_timeout1(void *bytes, uint32_t *nbytes);

static uint64_t thinca_initialize(struct thinca_impl * self, uint64_t val);
static uint64_t thinca_dispose(struct thinca_impl * self);
static uint64_t thinca_set_resource(struct thinca_impl * self, char * res);
static uint64_t thinca_set_pay_log(struct thinca_impl * self, uint64_t val, char * log, uint64_t val2, const char * size_lim);
static uint64_t thinca_set_client_log(struct thinca_impl * self, uint64_t val, char * log);
static uint64_t thinca_set_client_cfg(struct thinca_impl * self, char * log, uint64_t val);
static uint64_t thinca_set_goods_code(struct thinca_impl * self, char * code);
static uint64_t thinca_set_evt_handler(struct thinca_impl * self, void* handler);
static uint64_t thinca_set_cert(struct thinca_impl * self, char * cert, uint64_t val);
static uint64_t thinca_set_serial(struct thinca_impl * self, char * cert);
static uint64_t thinca_check_deal(struct thinca_impl * self, void* deal);
static uint64_t thinca_cancel(struct thinca_impl * self);
static uint64_t thinca_select(struct thinca_impl * self);
static uint64_t thinca_unk(struct thinca_impl * self, uint64_t val);
static void thinca_unk8(struct thinca_impl * self);

static uint64_t my_ThincaPaymentGetVersion();
static uint64_t (*next_ThincaPaymentGetVersion)();

static struct thinca_main* my_ThincaPaymentGetInstance(uint64_t ver);
static struct thinca_main* (*next_ThincaPaymentGetInstance)(uint64_t ver);

static struct thinca_main* thinca_stub;

static const struct reg_hook_val epay_adapter_keys[] = {
    {
        .name   = L"TfpsAimeRwAdapter",
        .read   = misc_read_thinca_adapter,
        .type   = REG_SZ,
    }
};

static const struct reg_hook_val epay_tcap_keys[] = {
    {
        .name   = L"CaLocation",
        .read   = misc_read_ca_loc,
        .type   = REG_SZ,
    },
    {
        .name   = L"ThincaTcapClientPath",
        .read   = misc_read_ca_client_loc,
        .type   = REG_SZ,
    },
    {
        .name   = L"ClientNetworkTimeout",
        .read   = misc_read_network_timeout,
        .type   = REG_DWORD,
    }
};

static const struct reg_hook_val epay_tcap_url0_keys[] = {
    {
        .name   = L"Pattern",
        .read   = misc_read_pattern0,
        .type   = REG_SZ,
    },
    {
        .name   = L"ClientNetworkTimeout",
        .read   = misc_read_network_timeout0,
        .type   = REG_DWORD,
    }
};

static const struct reg_hook_val epay_tcap_url1_keys[] = {
    {
        .name   = L"Pattern",
        .read   = misc_read_pattern1,
        .type   = REG_SZ,
    },
    {
        .name   = L"ClientNetworkTimeout",
        .read   = misc_read_network_timeout1,
        .type   = REG_DWORD,
    }
};

static const struct hook_symbol epay_syms[] = {
    {
        .name  = "ThincaPaymentGetVersion",
        .patch = my_ThincaPaymentGetVersion,
        .link  = (void **) &next_ThincaPaymentGetVersion,
        .ordinal = 1,
    }, 
    {
        .name  = "__imp_ThincaPaymentGetInstance",
        .patch = my_ThincaPaymentGetInstance,
        .link  = (void **) &next_ThincaPaymentGetInstance,
        .ordinal = 2,
    },
    {
        .name  = "ThincaPaymentGetInstance",
        .patch = my_ThincaPaymentGetInstance,
        .link  = (void **) &next_ThincaPaymentGetInstance,
        .ordinal = 2,
    }
};

HRESULT epay_apply_registry_hooks(){
    HRESULT hr = reg_hook_push_key(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\TFPaymentService\\ThincaRwAdapter",
            epay_adapter_keys,
            _countof(epay_adapter_keys));

    if (FAILED(hr)) {
        return hr;
    }

    hr = reg_hook_push_key(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\TFPaymentService\\ThincaTcapClient",
            epay_tcap_keys,
            _countof(epay_tcap_keys));

    if (FAILED(hr)) {
        return hr;
    }

    hr = reg_hook_push_key(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\TFPaymentService\\ThincaTcapClient\\URL0",
            epay_tcap_url0_keys,
            _countof(epay_tcap_url0_keys));

    if (FAILED(hr)) {
        return hr;
    }

    hr = reg_hook_push_key(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\TFPaymentService\\ThincaTcapClient\\URL1",
            epay_tcap_url1_keys,
            _countof(epay_tcap_url1_keys));

    return hr;
}

HRESULT epay_hook_init(const struct epay_config *cfg) {
    HRESULT hr;
    assert(cfg != NULL);

    if (!cfg->enable) {
        return S_FALSE;
    }

    hr = epay_apply_registry_hooks();
    if (FAILED(hr)){
        return hr;
    }

    dprintf("EPay: Registry initialized\n");

    // HACK:(?) the DLLs are loaded dynamically so we just preload it and apply DNS and VFS hooks to it
    HMODULE thincahttpclient = LoadLibraryA("thincahttpclient.dll");
    if (thincahttpclient != NULL){
        dns_hook_apply_hooks(thincahttpclient);
        path_hook_insert_hooks(thincahttpclient);
    }
    HMODULE thincapayment = LoadLibraryA("ThincaPayment.dll");
    if (thincapayment != NULL){
        path_hook_insert_hooks(thincapayment);
    }
    HMODULE thincatcapclient = LoadLibraryA("thincatcapclient.dll");
    if (thincatcapclient != NULL){
        path_hook_insert_hooks(thincatcapclient);
    }

    if (cfg->hook) {
        hook_table_apply(
                NULL,
                "ThincaPayment.dll",
                epay_syms,
                _countof(epay_syms));

        thinca_stub = (struct thinca_main *) malloc(sizeof(struct thinca_main));
        thinca_stub->impl1 = (struct thinca_impl *) malloc(sizeof(struct thinca_impl));

        thinca_stub->impl1->unk8 = thinca_unk8;
        thinca_stub->impl1->initialize = thinca_initialize;
        thinca_stub->impl1->dispose = thinca_dispose;
        thinca_stub->impl1->setResource = thinca_set_resource;
        thinca_stub->impl1->setThincaPaymentLog = thinca_set_pay_log;
        thinca_stub->impl1->setThincaEventInterface = thinca_set_evt_handler;
        thinca_stub->impl1->setIcasClientLog = thinca_set_client_log;
        thinca_stub->impl1->setIcasClientConfig = thinca_set_client_cfg;
        thinca_stub->impl1->setGoodsCode = thinca_set_goods_code;
        thinca_stub->impl1->setTerminalSerial = thinca_set_serial;
        thinca_stub->impl1->setClientCertificate = thinca_set_cert;
        thinca_stub->impl1->checkDeal = thinca_check_deal;
        thinca_stub->impl1->cancelRequest = thinca_cancel;
        thinca_stub->impl1->selectButton = thinca_select;
        thinca_stub->impl1->unk220 = thinca_unk;
        thinca_stub->impl1->unk228 = thinca_unk;

        dprintf("Epay: Hooks initialized\n");
    }

    return hr;
}

static HRESULT misc_read_thinca_adapter(void *bytes, uint32_t *nbytes)
{
    return reg_hook_read_wstr(bytes, nbytes, L"aime_rw_adapterMD.dll");
}

static HRESULT misc_read_ca_loc(void *bytes, uint32_t *nbytes)
{
    return reg_hook_read_wstr(bytes, nbytes, L"ca.pem");
}

static HRESULT misc_read_ca_client_loc(void *bytes, uint32_t *nbytes)
{
    return reg_hook_read_wstr(bytes, nbytes, L"thincatcapclient.dll");
}

static HRESULT misc_read_network_timeout(void *bytes, uint32_t *nbytes)
{
    return reg_hook_read_u32(bytes, nbytes, 20000);
}

static HRESULT misc_read_pattern0(void *bytes, uint32_t *nbytes)
{
    return reg_hook_read_wstr(bytes, nbytes, L".*\\.jsp");
}

static HRESULT misc_read_network_timeout0(void *bytes, uint32_t *nbytes)
{
    return reg_hook_read_u32(bytes, nbytes, 5000);
}

static HRESULT misc_read_pattern1(void *bytes, uint32_t *nbytes)
{
    return reg_hook_read_wstr(bytes, nbytes, L".*(closing|remove).*");
}

static HRESULT misc_read_network_timeout1(void *bytes, uint32_t *nbytes)
{
    return reg_hook_read_u32(bytes, nbytes, 60000);
}

static uint64_t thinca_initialize(struct thinca_impl * self, uint64_t val)
{
    dprintf("Epay: Thinca Initialize %lld\n", val);
    return 0;
}

static uint64_t thinca_dispose(struct thinca_impl * self)
{
    dprintf("Epay: Thinca Dispose\n");
    return 0;
}

static uint64_t thinca_set_resource(struct thinca_impl * self, char * res)
{
    dprintf("Epay: Thinca Set Resource %s\n", res);
    return 0;
}

static uint64_t thinca_set_pay_log(struct thinca_impl * self, uint64_t val, char * log, uint64_t val2, const char * size_lim)
{
    dprintf("Epay: Thinca Set Paylog %lld | %s | %lld | %s\n", val, log, val2, size_lim);
    return 0;
}

static uint64_t thinca_set_client_log(struct thinca_impl * self, uint64_t val, char * log)
{
    dprintf("Epay: Thinca Set ICAS Client log %lld | %s\n", val, log);
    return 0;
}

static uint64_t thinca_set_client_cfg(struct thinca_impl * self, char * log, uint64_t val)
{
    dprintf("Epay: Thinca Set ICAS Client Config %s | %lld\n", log, val);
    return 0;
}

static uint64_t thinca_set_goods_code(struct thinca_impl * self, char * code)
{
    dprintf("Epay: Thinca Set Goods Code %s\n", code);
    return 0;
}

static uint64_t thinca_set_evt_handler(struct thinca_impl * self, void* handler)
{
    dprintf("Epay: Thinca Set Event Handler %p\n", handler);
    return 0;
}

static uint64_t thinca_set_cert(struct thinca_impl * self, char * cert, uint64_t val)
{
    dprintf("Epay: Thinca Set Client Cert %s | %lld\n", cert, val);
    return 0;
}

static uint64_t thinca_set_serial(struct thinca_impl * self, char * cert)
{
    dprintf("Epay: Thinca Set Terminal Serial %s\n", cert);
    return 0;
}

static uint64_t thinca_check_deal(struct thinca_impl * self, void* deal)
{
    dprintf("Epay: Thinca Check Deal %p\n", deal);
    return 0;
}

static uint64_t thinca_cancel(struct thinca_impl * self)
{
    dprintf("Epay: Thinca Cancel\n");
    return 0;
}

static uint64_t thinca_select(struct thinca_impl * self)
{
    dprintf("Epay: Thinca Select\n");
    return 0;
}

static uint64_t thinca_unk(struct thinca_impl * self, uint64_t val)
{
    dprintf("Epay: Thinca Unknown 220/228 %lld\n", val);
    return 0;
}

static void thinca_unk8(struct thinca_impl * self)
{
    dprintf("Epay: Thinca Unknown 8\n");
}

static uint64_t my_ThincaPaymentGetVersion()
{
    return 0x1040B00;
}

static struct thinca_main* my_ThincaPaymentGetInstance(uint64_t ver)
{
    dprintf("Epay: my_ThincaPaymentGetInstance hit!\n");
    return thinca_stub;
}