#include <windows.h>
#include <d3d9.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "hook/com-proxy.h"
#include "hook/table.h"

#include "hooklib/dll.h"

#include "gfxhook/gfx.h"
#include "gfxhook/util.h"

#include "util/dprintf.h"

typedef IDirect3D9 * (WINAPI *Direct3DCreate9_t)(UINT sdk_ver);
typedef HRESULT (WINAPI *Direct3DCreate9Ex_t)(UINT sdk_ver, IDirect3D9Ex **d3d9ex);

static HRESULT STDMETHODCALLTYPE my_IDirect3D9_CreateDevice(
        IDirect3D9 *self,
        UINT adapter,
        D3DDEVTYPE type,
        HWND hwnd,
        DWORD flags,
        D3DPRESENT_PARAMETERS *pp,
        IDirect3DDevice9 **pdev);
static HRESULT STDMETHODCALLTYPE my_IDirect3D9Ex_CreateDevice(
        IDirect3D9Ex *self,
        UINT adapter,
        D3DDEVTYPE type,
        HWND hwnd,
        DWORD flags,
        D3DPRESENT_PARAMETERS *pp,
        IDirect3DDevice9 **pdev);
static HRESULT STDMETHODCALLTYPE my_IDirect3DDevice9_Reset(
        IDirect3DDevice9 *self,
        D3DPRESENT_PARAMETERS *pp);
static HRESULT STDMETHODCALLTYPE my_IDirect3DDevice9_Present(
        IDirect3DDevice9 *self,
        const RECT *src,
        const RECT *dest,
        HWND hwnd,
        const RGNDATA *dirty);
static HRESULT gfx_d3d9_wrap_device(
        IDirect3DDevice9 **pdev,
        const D3DPRESENT_PARAMETERS *pp,
        bool ex);
static HRESULT gfx_d3d9_device_recover(struct com_proxy *proxy);
static void gfx_d3d9_device_context_free(void *ctx);

struct gfx_d3d9_device_context {
    D3DPRESENT_PARAMETERS pp;
    bool have_pp;
    bool ex;
};

static struct gfx_config gfx_config;
static Direct3DCreate9_t next_Direct3DCreate9;
static Direct3DCreate9Ex_t next_Direct3DCreate9Ex;

static const struct hook_symbol gfx_hooks[] = {
    {
        .name   = "Direct3DCreate9",
        .patch  = Direct3DCreate9,
        .link   = (void **) &next_Direct3DCreate9,
    }, {
        .name   = "Direct3DCreate9Ex",
        .patch  = Direct3DCreate9Ex,
        .link   = (void **) &next_Direct3DCreate9Ex,
    },
};

void gfx_d3d9_hook_init(const struct gfx_config *cfg, HINSTANCE self)
{
    HMODULE d3d9;

    assert(cfg != NULL);

    if (!cfg->enable) {
        return;
    }

    memcpy(&gfx_config, cfg, sizeof(*cfg));
    hook_table_apply(NULL, "d3d9.dll", gfx_hooks, _countof(gfx_hooks));

    if (next_Direct3DCreate9 == NULL || next_Direct3DCreate9Ex == NULL) {
        d3d9 = LoadLibraryW(L"d3d9.dll");

        if (d3d9 == NULL) {
            dprintf("Gfx: d3d9.dll not found or failed initialization\n");

            goto fail;
        }

        if (next_Direct3DCreate9 == NULL) {
            next_Direct3DCreate9 = (Direct3DCreate9_t) GetProcAddress(d3d9, "Direct3DCreate9");
        }
        if (next_Direct3DCreate9Ex == NULL) {
            next_Direct3DCreate9Ex = (Direct3DCreate9Ex_t) GetProcAddress(d3d9, "Direct3DCreate9Ex");
        }

        if (next_Direct3DCreate9 == NULL) {
            dprintf("Gfx: Direct3DCreate9 not found in loaded d3d9.dll\n");

            goto fail;
        }
        if (next_Direct3DCreate9Ex == NULL) {
            dprintf("Gfx: Direct3DCreate9Ex not found in loaded d3d9.dll\n");

            goto fail;
        }
    }

    if (self != NULL) {
        dll_hook_push(self, L"d3d9.dll");
    }

    return;

fail:
    if (d3d9 != NULL) {
        FreeLibrary(d3d9);
    }
}

IDirect3D9 * WINAPI Direct3DCreate9(UINT sdk_ver)
{
    struct com_proxy *proxy;
    IDirect3D9Vtbl *vtbl;
    IDirect3D9Ex *api_ex;
    IDirect3D9 *api;
    HRESULT hr;

    dprintf("Gfx: Direct3DCreate9 hook hit\n");

    api = NULL;
    api_ex = NULL;

    if (next_Direct3DCreate9 == NULL) {
        dprintf("Gfx: next_Direct3DCreate9 == NULL\n");

        goto fail;
    }

    if (next_Direct3DCreate9Ex != NULL && !gfx_config.windowed) {
        hr = next_Direct3DCreate9Ex(sdk_ver, &api_ex);

        if (SUCCEEDED(hr)) {
            dprintf("Gfx: Using Direct3DCreate9Ex for fullscreen D3D9 device\n");

            api = (IDirect3D9 *) api_ex;
        } else {
            dprintf(
                    "Gfx: Direct3DCreate9Ex returned %x, falling back to Direct3DCreate9\n",
                    (int) hr);
        }
    }

    if (api == NULL) {
        api = next_Direct3DCreate9(sdk_ver);
    }

    if (api == NULL) {
        dprintf("Gfx: next_Direct3DCreate9 returned NULL\n");

        goto fail;
    }

    hr = com_proxy_wrap(&proxy, api, sizeof(*api->lpVtbl));

    if (FAILED(hr)) {
        dprintf("Gfx: com_proxy_wrap returned %x\n", (int) hr);

        goto fail;
    }

    vtbl = proxy->vptr;
    vtbl->CreateDevice = my_IDirect3D9_CreateDevice;
    proxy->ctx = api_ex != NULL ? proxy : NULL;

    return (IDirect3D9 *) proxy;

fail:
    if (api != NULL) {
        IDirect3D9_Release(api);
    }

    return NULL;
}

HRESULT WINAPI Direct3DCreate9Ex(UINT sdk_ver, IDirect3D9Ex **d3d9ex)
{
    struct com_proxy *proxy;
    IDirect3D9ExVtbl *vtbl;
    IDirect3D9Ex *api;
    HRESULT hr;

    dprintf("Gfx: Direct3DCreate9Ex hook hit\n");

    api = NULL;

    if (next_Direct3DCreate9Ex == NULL) {
        dprintf("Gfx: next_Direct3DCreate9Ex == NULL\n");

        goto fail;
    }

    hr = next_Direct3DCreate9Ex(sdk_ver, d3d9ex);

    if (FAILED(hr)) {
        dprintf("Gfx: next_Direct3DCreate9Ex returned %x\n", (int) hr);

        goto fail;
    }

    api = *d3d9ex;
    hr = com_proxy_wrap(&proxy, api, sizeof(*api->lpVtbl));

    if (FAILED(hr)) {
        dprintf("Gfx: com_proxy_wrap returned %x\n", (int) hr);

        goto fail;
    }

    vtbl = proxy->vptr;
    vtbl->CreateDevice = my_IDirect3D9Ex_CreateDevice;
    proxy->ctx = proxy;

    *d3d9ex = (IDirect3D9Ex *) proxy;

    return S_OK;

fail:
    if (api != NULL) {
        IDirect3D9Ex_Release(api);
    }

    return hr;
}

static HRESULT STDMETHODCALLTYPE my_IDirect3D9_CreateDevice(
        IDirect3D9 *self,
        UINT adapter,
        D3DDEVTYPE type,
        HWND hwnd,
        DWORD flags,
        D3DPRESENT_PARAMETERS *pp,
        IDirect3DDevice9 **pdev)
{
    struct com_proxy *proxy;
    IDirect3D9 *real;
    HRESULT hr;
    bool ex;

    dprintf("Gfx: IDirect3D9::CreateDevice hook hit\n");

    proxy = com_proxy_downcast(self);
    real = proxy->real;
    ex = proxy->ctx != NULL;

    if (gfx_config.windowed) {
        pp->Windowed = TRUE;
        pp->FullScreen_RefreshRateInHz = 0;
    }

    if (gfx_config.framed) {
        gfx_util_frame_window(hwnd);
    }

    UINT max_adapter = IDirect3D9_GetAdapterCount(real);
    adapter = gfx_config.monitor;
    if (adapter >= max_adapter) {
        dprintf(
            "Gfx: Requested adapter %d but maximum is %d. Using primary monitor\n",
            gfx_config.monitor, max_adapter - 1
        );
        adapter = D3DADAPTER_DEFAULT;
    } else {
        dprintf("Gfx: Using adapter %d\n", gfx_config.monitor);
    }

    hr = IDirect3D9_CreateDevice(real, adapter, type, hwnd, flags, pp, pdev);

    if (SUCCEEDED(hr)) {
        hr = gfx_d3d9_wrap_device(pdev, pp, ex);
    }

    return hr;
}

static HRESULT STDMETHODCALLTYPE my_IDirect3D9Ex_CreateDevice(
        IDirect3D9Ex *self,
        UINT adapter,
        D3DDEVTYPE type,
        HWND hwnd,
        DWORD flags,
        D3DPRESENT_PARAMETERS *pp,
        IDirect3DDevice9 **pdev)
{
    dprintf("Gfx: IDirect3D9Ex::CreateDevice hook forwarding to my_IDirect3D9_CreateDevice\n");

    return my_IDirect3D9_CreateDevice(
            (IDirect3D9 *) self,
            adapter,
            type,
            hwnd,
            flags,
            pp,
            pdev);
}

static HRESULT gfx_d3d9_wrap_device(
        IDirect3DDevice9 **pdev,
        const D3DPRESENT_PARAMETERS *pp,
        bool ex)
{
    struct gfx_d3d9_device_context *ctx;
    struct com_proxy *proxy;
    IDirect3DDevice9Vtbl *vtbl;
    HRESULT hr;

    assert(pdev != NULL);

    if (*pdev == NULL) {
        return S_OK;
    }

    ctx = calloc(1, sizeof(*ctx));

    if (ctx == NULL) {
        return E_OUTOFMEMORY;
    }

    if (pp != NULL) {
        memcpy(&ctx->pp, pp, sizeof(ctx->pp));
        ctx->have_pp = true;
    }

    ctx->ex = ex;

    hr = com_proxy_wrap(&proxy, *pdev, sizeof(*(*pdev)->lpVtbl));

    if (FAILED(hr)) {
        dprintf("Gfx: Device com_proxy_wrap returned %x\n", (int) hr);
        free(ctx);

        return hr;
    }

    proxy->ctx = ctx;
    proxy->cleanup_ctx = gfx_d3d9_device_context_free;

    vtbl = proxy->vptr;
    vtbl->Reset = my_IDirect3DDevice9_Reset;
    vtbl->Present = my_IDirect3DDevice9_Present;

    *pdev = (IDirect3DDevice9 *) proxy;

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE my_IDirect3DDevice9_Reset(
        IDirect3DDevice9 *self,
        D3DPRESENT_PARAMETERS *pp)
{
    struct gfx_d3d9_device_context *ctx;
    struct com_proxy *proxy;
    IDirect3DDevice9 *real;
    HRESULT hr;

    proxy = com_proxy_downcast(self);
    real = proxy->real;
    ctx = proxy->ctx;

    hr = IDirect3DDevice9_Reset(real, pp);

    if (SUCCEEDED(hr) && ctx != NULL && pp != NULL) {
        memcpy(&ctx->pp, pp, sizeof(ctx->pp));
        ctx->have_pp = true;
    }

    return hr;
}

static HRESULT STDMETHODCALLTYPE my_IDirect3DDevice9_Present(
        IDirect3DDevice9 *self,
        const RECT *src,
        const RECT *dest,
        HWND hwnd,
        const RGNDATA *dirty)
{
    struct com_proxy *proxy;
    IDirect3DDevice9 *real;
    struct gfx_d3d9_device_context *ctx;
    HRESULT hr;

    proxy = com_proxy_downcast(self);
    real = proxy->real;
    ctx = proxy->ctx;

    if (ctx != NULL && ctx->ex) {
        hr = IDirect3DDevice9Ex_PresentEx(
                (IDirect3DDevice9Ex *) real,
                src,
                dest,
                hwnd,
                dirty,
                0);
    } else {
        hr = IDirect3DDevice9_Present(real, src, dest, hwnd, dirty);
    }

    if (hr == D3DERR_DEVICELOST) {
        dprintf("Gfx: D3D9 device lost, attempting reset\n");

        return gfx_d3d9_device_recover(proxy);
    }

    return hr;
}

static HRESULT gfx_d3d9_device_recover(struct com_proxy *proxy)
{
    struct gfx_d3d9_device_context *ctx;
    IDirect3DDevice9 *real;
    HRESULT hr;

    assert(proxy != NULL);

    real = proxy->real;
    ctx = proxy->ctx;

    if (ctx == NULL || !ctx->have_pp) {
        return D3DERR_DEVICELOST;
    }

    hr = IDirect3DDevice9_TestCooperativeLevel(real);

    while (hr == D3DERR_DEVICELOST) {
        Sleep(10);
        hr = IDirect3DDevice9_TestCooperativeLevel(real);
    }

    if (hr != D3DERR_DEVICENOTRESET) {
        return D3DERR_DEVICELOST;
    }

    hr = IDirect3DDevice9_Reset(real, &ctx->pp);

    if (hr == D3DERR_INVALIDCALL) {
        return D3DERR_DEVICELOST;
    }

    if (FAILED(hr)) {
        return hr;
    }

    dprintf("Gfx: D3D9 device reset successfully\n");

    return D3D_OK;
}

static void gfx_d3d9_device_context_free(void *ctx)
{
    free(ctx);
}
