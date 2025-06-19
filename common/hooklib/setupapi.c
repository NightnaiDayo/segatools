#include <windows.h>
#include <setupapi.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "hook/table.h"

#include "hooklib/setupapi.h"

#include "util/dprintf.h"

struct setupapi_class {
    const GUID *guid;
    const wchar_t *path;
    char *a_path;
    HDEVINFO cur_handle;
};

static void setupapi_hook_init(void);

/* API hooks */

static HDEVINFO WINAPI my_SetupDiGetClassDevsW(
        const GUID *ClassGuid,
        wchar_t *Enumerator,
        HWND hwndParent,
        DWORD Flags);

static HDEVINFO WINAPI my_SetupDiGetClassDevsA(
        const GUID *ClassGuid,
        char *Enumerator,
        HWND hwndParent,
        DWORD Flags);

static BOOL WINAPI my_SetupDiEnumDeviceInterfaces(
        HDEVINFO DeviceInfoSet,
        SP_DEVINFO_DATA *DeviceInfoData,
        const GUID *InterfaceClassGuid,
        DWORD MemberIndex,
        SP_DEVICE_INTERFACE_DATA *DeviceInterfaceData);

static BOOL WINAPI my_SetupDiGetDeviceInterfaceDetailW(
        HDEVINFO DeviceInfoSet,
        SP_DEVICE_INTERFACE_DATA *DeviceInterfaceData,
        SP_DEVICE_INTERFACE_DETAIL_DATA_W *DeviceInterfaceDetailData,
        DWORD DeviceInterfaceDetailDataSize,
        DWORD *RequiredSize,
        SP_DEVINFO_DATA *DeviceInfoData);

static BOOL WINAPI my_SetupDiGetDeviceInterfaceDetailA(
        HDEVINFO DeviceInfoSet,
        SP_DEVICE_INTERFACE_DATA *DeviceInterfaceData,
        SP_DEVICE_INTERFACE_DETAIL_DATA_A *DeviceInterfaceDetailData,
        DWORD DeviceInterfaceDetailDataSize,
        DWORD *RequiredSize,
        SP_DEVINFO_DATA *DeviceInfoData);

static BOOL WINAPI my_SetupDiDestroyDeviceInfoList(HDEVINFO DeviceInfoSet);

/* Links */

static HDEVINFO (WINAPI *next_SetupDiGetClassDevsW)(
        const GUID *ClassGuid,
        wchar_t *Enumerator,
        HWND hwndParent,
        DWORD Flags);

static HDEVINFO (WINAPI *next_SetupDiGetClassDevsA)(
        const GUID *ClassGuid,
        char *Enumerator,
        HWND hwndParent,
        DWORD Flags);

static BOOL (WINAPI *next_SetupDiEnumDeviceInterfaces)(
        HDEVINFO DeviceInfoSet,
        SP_DEVINFO_DATA *DeviceInfoData,
        const GUID *InterfaceClassGuid,
        DWORD MemberIndex,
        SP_DEVICE_INTERFACE_DATA *DeviceInterfaceData);

static BOOL (WINAPI *next_SetupDiGetDeviceInterfaceDetailW)(
        HDEVINFO DeviceInfoSet,
        SP_DEVICE_INTERFACE_DATA *DeviceInterfaceData,
        SP_DEVICE_INTERFACE_DETAIL_DATA_W *DeviceInterfaceDetailData,
        DWORD DeviceInterfaceDetailDataSize,
        DWORD *RequiredSize,
        SP_DEVINFO_DATA *DeviceInfoData);

static BOOL (WINAPI *next_SetupDiGetDeviceInterfaceDetailA)(
        HDEVINFO DeviceInfoSet,
        SP_DEVICE_INTERFACE_DATA *DeviceInterfaceData,
        SP_DEVICE_INTERFACE_DETAIL_DATA_A *DeviceInterfaceDetailData,
        DWORD DeviceInterfaceDetailDataSize,
        DWORD *RequiredSize,
        SP_DEVINFO_DATA *DeviceInfoData);

static BOOL (WINAPI *next_SetupDiDestroyDeviceInfoList)(HDEVINFO DeviceInfoSet);

/* Hook tbl */

static const struct hook_symbol setupapi_syms[] = {
    {
        .name   = "SetupDiGetClassDevsW",
        .patch  = my_SetupDiGetClassDevsW,
        .link   = (void *) &next_SetupDiGetClassDevsW,
    }, {
        .name   = "SetupDiGetClassDevsA",
        .patch  = my_SetupDiGetClassDevsA,
        .link   = (void *) &next_SetupDiGetClassDevsA,
    }, {
        .name   = "SetupDiEnumDeviceInterfaces",
        .patch  = my_SetupDiEnumDeviceInterfaces,
        .link   = (void *) &next_SetupDiEnumDeviceInterfaces,
    }, {
        .name   = "SetupDiGetDeviceInterfaceDetailW",
        .patch  = my_SetupDiGetDeviceInterfaceDetailW,
        .link   = (void *) &next_SetupDiGetDeviceInterfaceDetailW,
    }, {
        .name   = "SetupDiGetDeviceInterfaceDetailA",
        .patch  = my_SetupDiGetDeviceInterfaceDetailA,
        .link   = (void *) &next_SetupDiGetDeviceInterfaceDetailA,
    }, {
        .name   = "SetupDiDestroyDeviceInfoList",
        .patch  = my_SetupDiDestroyDeviceInfoList,
        .link   = (void *) &next_SetupDiDestroyDeviceInfoList,
    }
};

static bool setupapi_initted;
static CRITICAL_SECTION setupapi_lock;
static struct setupapi_class *setupapi_classes;
static size_t setupapi_nclasses;

HRESULT setupapi_add_phantom_dev(const GUID *iface_class, const wchar_t *path)
{
    struct setupapi_class *class_;
    struct setupapi_class *new_array;
    size_t a_path_len;
    HRESULT hr;

    assert(iface_class != NULL);
    assert(path != NULL);

    setupapi_hook_init();

    EnterCriticalSection(&setupapi_lock);

    new_array = realloc(
            setupapi_classes,
            (setupapi_nclasses + 1) * sizeof(struct setupapi_class));

    if (new_array == NULL) {
        hr = E_OUTOFMEMORY;

        goto end;
    }

    setupapi_classes = new_array;

    class_ = &setupapi_classes[setupapi_nclasses++];
    class_->guid = iface_class;
    class_->path = path;

    a_path_len = wcslen(path) * sizeof(wchar_t) + 1;
    class_->a_path = (char*)malloc(a_path_len);
    wcstombs(class_->a_path, path, a_path_len);

    hr = S_OK;

end:
    LeaveCriticalSection(&setupapi_lock);

    return hr;
}

static void setupapi_hook_init()
{
    if (setupapi_initted) {
        return;
    }

    setupapi_hook_insert_hooks(NULL);

    InitializeCriticalSection(&setupapi_lock);
    setupapi_initted = true;
}

void setupapi_hook_insert_hooks(HMODULE target)
{
    hook_table_apply(
            target,
            "setupapi.dll",
            setupapi_syms,
            _countof(setupapi_syms));
}

static HDEVINFO WINAPI my_SetupDiGetClassDevsW(
        const GUID *ClassGuid,
        wchar_t *Enumerator,
        HWND hwndParent,
        DWORD Flags)
{
    struct setupapi_class *class_;
    HDEVINFO result;
    size_t i;

    result = next_SetupDiGetClassDevsW(
            ClassGuid,
            Enumerator,
            hwndParent,
            Flags);

    if (result == INVALID_HANDLE_VALUE || ClassGuid == NULL) {
        return result;
    }

    EnterCriticalSection(&setupapi_lock);

    for (i = 0 ; i < setupapi_nclasses ; i++) {
        class_ = &setupapi_classes[i];
        if (memcmp(ClassGuid, class_->guid, sizeof(*ClassGuid)) == 0) {
            class_->cur_handle = result;
        }
    }

    LeaveCriticalSection(&setupapi_lock);

    return result;
}

static HDEVINFO WINAPI my_SetupDiGetClassDevsA(
        const GUID *ClassGuid,
        char *Enumerator,
        HWND hwndParent,
        DWORD Flags)
{
    struct setupapi_class *class_;
    HDEVINFO result;
    size_t i;

    result = next_SetupDiGetClassDevsA(
            ClassGuid,
            Enumerator,
            hwndParent,
            Flags);

    if (result == INVALID_HANDLE_VALUE || ClassGuid == NULL) {
        return result;
    }

    EnterCriticalSection(&setupapi_lock);

    for (i = 0 ; i < setupapi_nclasses ; i++) {
        class_ = &setupapi_classes[i];
        if (memcmp(ClassGuid, class_->guid, sizeof(*ClassGuid)) == 0) {
            class_->cur_handle = result;
        }
    }

    LeaveCriticalSection(&setupapi_lock);

    return result;
}

static BOOL WINAPI my_SetupDiEnumDeviceInterfaces(
        HDEVINFO DeviceInfoSet,
        SP_DEVINFO_DATA *DeviceInfoData,
        const GUID *InterfaceClassGuid,
        DWORD MemberIndex,
        SP_DEVICE_INTERFACE_DATA *DeviceInterfaceData)
{
    const struct setupapi_class *class_;
    size_t i;

    if (    DeviceInfoSet == INVALID_HANDLE_VALUE ||
            DeviceInterfaceData == NULL ||
            DeviceInterfaceData->cbSize != sizeof(*DeviceInterfaceData)) {
        goto pass;
    }

    if (MemberIndex > 0) {
        MemberIndex--;

        goto pass;
    }

    EnterCriticalSection(&setupapi_lock);

    for (   i = 0, class_ = NULL ;
            i < setupapi_nclasses && class_ == NULL ;
            i++) {
        if (DeviceInfoSet == setupapi_classes[i].cur_handle) {
            class_ = &setupapi_classes[i];

            dprintf("SetupAPI: Interface {%08lx-...} -> Device node %S\n",
                    class_->guid->Data1,
                    class_->path);

            memcpy( &DeviceInterfaceData->InterfaceClassGuid,
                    class_->guid,
                    sizeof(GUID));
            DeviceInterfaceData->Flags = SPINT_ACTIVE;
            DeviceInterfaceData->Reserved = (ULONG_PTR) class_->path;
        }
    }

    LeaveCriticalSection(&setupapi_lock);

    if (class_ == NULL) {
        goto pass;
    }

    SetLastError(ERROR_SUCCESS);

    return TRUE;

pass:
    return next_SetupDiEnumDeviceInterfaces(
            DeviceInfoSet,
            DeviceInfoData,
            InterfaceClassGuid,
            MemberIndex,
            DeviceInterfaceData);
}

static BOOL WINAPI my_SetupDiGetDeviceInterfaceDetailW(
        HDEVINFO DeviceInfoSet,
        SP_DEVICE_INTERFACE_DATA *DeviceInterfaceData,
        SP_DEVICE_INTERFACE_DETAIL_DATA_W *DeviceInterfaceDetailData,
        DWORD DeviceInterfaceDetailDataSize,
        DWORD *RequiredSize,
        SP_DEVINFO_DATA *DeviceInfoData)
{
    const wchar_t *wstr;
    size_t nbytes_wstr;
    size_t nbytes_total;
    size_t i;
    bool match;

    if (DeviceInfoSet == INVALID_HANDLE_VALUE || DeviceInterfaceData == NULL) {
        goto pass;
    }

    EnterCriticalSection(&setupapi_lock);

    for (   i = 0, match = false ;
            i < setupapi_nclasses && !match ;
            i++) {
        if (    DeviceInfoSet == setupapi_classes[i].cur_handle &&
                DeviceInterfaceData->Reserved == (ULONG_PTR) setupapi_classes[i].path) {
            match = true;
        }
    }

    LeaveCriticalSection(&setupapi_lock);

    if (!match) {
        goto pass;
    }

    wstr = (const wchar_t *) DeviceInterfaceData->Reserved;
    nbytes_wstr = (wcslen(wstr) + 1) * sizeof(wchar_t);
    nbytes_total  = offsetof(SP_DEVICE_INTERFACE_DETAIL_DATA_W, DevicePath);
    nbytes_total += nbytes_wstr;

    if (RequiredSize != NULL) {
        *RequiredSize = (DWORD) nbytes_total;
    }

    if (    DeviceInterfaceDetailData == NULL &&
            DeviceInterfaceDetailDataSize < nbytes_total) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);

        return FALSE;
    }

    if (DeviceInterfaceDetailData->cbSize!=sizeof(*DeviceInterfaceDetailData)) {
        SetLastError(ERROR_INVALID_PARAMETER);

        return FALSE;
    }

    memcpy(DeviceInterfaceDetailData->DevicePath, wstr, nbytes_wstr);
    SetLastError(ERROR_SUCCESS);

    return TRUE;

pass:
    return next_SetupDiGetDeviceInterfaceDetailW(
            DeviceInfoSet,
            DeviceInterfaceData,
            DeviceInterfaceDetailData,
            DeviceInterfaceDetailDataSize,
            RequiredSize,
            DeviceInfoData);
}

static BOOL WINAPI my_SetupDiGetDeviceInterfaceDetailA(
        HDEVINFO DeviceInfoSet,
        SP_DEVICE_INTERFACE_DATA *DeviceInterfaceData,
        SP_DEVICE_INTERFACE_DETAIL_DATA_A *DeviceInterfaceDetailData,
        DWORD DeviceInterfaceDetailDataSize,
        DWORD *RequiredSize,
        SP_DEVINFO_DATA *DeviceInfoData)
{
    const char *str;
    size_t nbytes_str;
    size_t nbytes_total;
    size_t i;
    bool match;

    if (DeviceInfoSet == INVALID_HANDLE_VALUE || DeviceInterfaceData == NULL) {
        goto pass;
    }

    EnterCriticalSection(&setupapi_lock);

    for (i = 0, match = false; i < setupapi_nclasses && !match; i++) {
        if (DeviceInfoSet == setupapi_classes[i].cur_handle &&
            DeviceInterfaceData->Reserved == (ULONG_PTR) setupapi_classes[i].path) {
            str = setupapi_classes[i].a_path;
            match = true;
        }
    }

    LeaveCriticalSection(&setupapi_lock);

    if (!match) {
        goto pass;
    }

    nbytes_str = strlen(str) + 1;
    nbytes_total  = offsetof(SP_DEVICE_INTERFACE_DETAIL_DATA_W, DevicePath);
    nbytes_total += nbytes_str;

    if (RequiredSize != NULL) {
        *RequiredSize = (DWORD) nbytes_total;
    }

    if (    DeviceInterfaceDetailData == NULL &&
            DeviceInterfaceDetailDataSize < nbytes_total) {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);

        return FALSE;
    }

    if (DeviceInterfaceDetailData->cbSize!=sizeof(*DeviceInterfaceDetailData)) {
        SetLastError(ERROR_INVALID_PARAMETER);

        return FALSE;
    }

    memcpy(DeviceInterfaceDetailData->DevicePath, str, nbytes_str);
    SetLastError(ERROR_SUCCESS);

    return TRUE;

    pass:
    return next_SetupDiGetDeviceInterfaceDetailA(
            DeviceInfoSet,
            DeviceInterfaceData,
            DeviceInterfaceDetailData,
            DeviceInterfaceDetailDataSize,
            RequiredSize,
            DeviceInfoData);
}

static BOOL WINAPI my_SetupDiDestroyDeviceInfoList(HDEVINFO DeviceInfoSet)
{
    size_t i;

    EnterCriticalSection(&setupapi_lock);

    for (i = 0 ; i < setupapi_nclasses ; i++) {
        if (setupapi_classes[i].cur_handle == DeviceInfoSet) {
            setupapi_classes[i].cur_handle = NULL;
        }
    }

    LeaveCriticalSection(&setupapi_lock);

    return next_SetupDiDestroyDeviceInfoList(DeviceInfoSet);
}
