/* Might push this to capnhook, don't add any util dependencies. */

#include <windows.h>
#include <windns.h>
#include <ws2tcpip.h>
#include <winhttp.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hook/hr.h"
#include "hook/table.h"

#include "util/dprintf.h"
#include "util/get_function_ordinal.h"

#include "hooklib/dns.h"

/* Latest w32headers does not include DnsQueryEx, so we'll have to "polyfill"
   its associated data types here for the time being.

   Results and cancel handle are passed through, so we'll just use void
   pointers for those args. So are most of the fields in this structure, for
   that matter. */

typedef struct POLYFILL_DNS_QUERY_REQUEST {
    ULONG Version;
    PCWSTR QueryName;
    WORD QueryType;
    ULONG64 QueryOptions;
    void* pDnsServerList;
    ULONG InterfaceIndex;
    void* pQueryCompletionCallback;
    PVOID pQueryContext;
} POLYFILL_DNS_QUERY_REQUEST;

struct dns_hook_entry {
    wchar_t *from;
    wchar_t *to;
};

/* Hook funcs */

static DNS_STATUS WINAPI hook_DnsQuery_A(
        const char *pszName,
        WORD wType,
        DWORD Options,
        void *pExtra,
        DNS_RECORD **ppQueryResults,
        void *pReserved);

static DNS_STATUS WINAPI hook_DnsQuery_W(
        const wchar_t *pszName,
        WORD wType,
        DWORD Options,
        void *pExtra,
        DNS_RECORD **ppQueryResults,
        void *pReserved);

static DNS_STATUS WINAPI hook_DnsQueryEx(
        POLYFILL_DNS_QUERY_REQUEST *pRequest,
        void *pQueryResults,
        void *pCancelHandle);

static int WSAAPI hook_getaddrinfo(
        const char *pNodeName,
        const char *pServiceName,
        const ADDRINFOA *pHints,
        ADDRINFOA **ppResult);

static int WSAAPI hook_GetAddrInfoW(
        const wchar_t *pNodeName,
        const wchar_t *pServiceName,
        const ADDRINFOW *pHints,
        PADDRINFOW *ppResult);

static int WSAAPI hook_GetAddrInfoExA(
        const char *pName,
        const char *pServiceName,
        DWORD dwNameSpace,
        LPGUID lpNspId,
        const ADDRINFOEXW *hints,
        PADDRINFOEXA *ppResult,
        struct timeval *timeout,
        LPOVERLAPPED lpOverlapped,
        LPLOOKUPSERVICE_COMPLETION_ROUTINE lpCompletionRoutine,
        LPHANDLE lpHandle);

static int WSAAPI hook_GetAddrInfoExW(
        const wchar_t *pName,
        const wchar_t *pServiceName,
        DWORD dwNameSpace,
        LPGUID lpNspId,
        const ADDRINFOEXW *hints,
        PADDRINFOEXW *ppResult,
        struct timeval *timeout,
        LPOVERLAPPED lpOverlapped,
        LPLOOKUPSERVICE_COMPLETION_ROUTINE lpCompletionRoutine,
        LPHANDLE lpHandle);

static HINTERNET WINAPI hook_WinHttpConnect(
        HINTERNET hSession,
        const wchar_t *pwszServerName,
        INTERNET_PORT nServerPort,
        DWORD dwReserved);

static bool WINAPI hook_WinHttpCrackUrl(
        const wchar_t *pwszUrl,
        DWORD dwUrlLength,
        DWORD dwFlags,
        LPURL_COMPONENTS lpUrlComponents);

static DWORD WINAPI hook_send(
        SOCKET s,
        const char* buf,
        int len,
        int flags);

static int WINAPI hook_connect(
        SOCKET s,
        const struct sockaddr *name,
        int namelen);

/* Link pointers */

static DNS_STATUS (WINAPI *next_DnsQuery_A)(
        const char *pszName,
        WORD wType,
        DWORD Options,
        void *pExtra,
        DNS_RECORD **ppQueryResults,
        void *pReserved);

static DNS_STATUS (WINAPI *next_DnsQuery_W)(
        const wchar_t *pszName,
        WORD wType,
        DWORD Options,
        void *pExtra,
        DNS_RECORD **ppQueryResults,
        void *pReserved);

static DNS_STATUS (WINAPI *next_DnsQueryEx)(
        POLYFILL_DNS_QUERY_REQUEST *pRequest,
        void *pQueryResults,
        void *pCancelHandle);

static int (WSAAPI *next_getaddrinfo)(
        const char *pNodeName,
        const char *pServiceName,
        const ADDRINFOA *pHints,
        ADDRINFOA **ppResult);

static int (WSAAPI *next_GetAddrInfoW)(
        const wchar_t *pNodeName,
        const wchar_t *pServiceName,
        const ADDRINFOW *pHints,
        PADDRINFOW *ppResult);

static int (WSAAPI *next_GetAddrInfoExA)(
        const char *pName,
        const char *pServiceName,
        DWORD dwNameSpace,
        LPGUID lpNspId,
        const ADDRINFOEXW *hints,
        PADDRINFOEXA *ppResult,
        struct timeval *timeout,
        LPOVERLAPPED lpOverlapped,
        LPLOOKUPSERVICE_COMPLETION_ROUTINE lpCompletionRoutine,
        LPHANDLE lpHandle);

static int (WSAAPI *next_GetAddrInfoExW)(
        const wchar_t *pName,
        const wchar_t *pServiceName,
        DWORD dwNameSpace,
        LPGUID lpNspId,
        const ADDRINFOEXW *hints,
        PADDRINFOEXW *ppResult,
        struct timeval *timeout,
        LPOVERLAPPED lpOverlapped,
        LPLOOKUPSERVICE_COMPLETION_ROUTINE lpCompletionRoutine,
        LPHANDLE lpHandle);

static HINTERNET (WINAPI *next_WinHttpConnect)(
        HINTERNET hSession,
        const wchar_t *pwszServerName,
        INTERNET_PORT nServerPort,
        DWORD dwReserved);

static bool (WINAPI *next_WinHttpCrackUrl)(
        const wchar_t *pwszUrl,
        DWORD dwUrlLength,
        DWORD dwFlags,
        LPURL_COMPONENTS lpUrlComponents);

static DWORD (WINAPI *next_send)(
        SOCKET s,
        const char* buf,
        int len,
        int flags);

static int (__stdcall *next_connect)(
        SOCKET s,
        const struct sockaddr *name,
        int namelen);

static const struct hook_symbol dns_hook_syms_dnsapi[] = {
    {
        .name       = "DnsQuery_A",
        .patch      = hook_DnsQuery_A,
        .link       = (void **) &next_DnsQuery_A,
    }, {
        .name       = "DnsQuery_W",
        .patch      = hook_DnsQuery_W,
        .link       = (void **) &next_DnsQuery_W,
    }, {
        .name       = "DnsQueryEx",
        .patch      = hook_DnsQueryEx,
        .link       = (void **) &next_DnsQueryEx,
    }
};

static const struct hook_symbol dns_hook_syms_ws2[] = {
    {
        .name       = "getaddrinfo",
        .ordinal    = 176,
        .patch      = hook_getaddrinfo,
        .link       = (void **) &next_getaddrinfo,
    },
    {
        .name       = "GetAddrInfoW",
        .patch      = hook_GetAddrInfoW,
        .link       = (void **) &next_GetAddrInfoW,
    },
    {
        .name       = "GetAddrInfoExA",
        .patch      = hook_GetAddrInfoExA,
        .link       = (void **) &next_GetAddrInfoExA,
    },
    {
        .name       = "GetAddrInfoExW",
        .patch      = hook_GetAddrInfoExW,
        .link       = (void **) &next_GetAddrInfoExW,
    }
};

static const struct hook_symbol dns_hook_syms_winhttp[] = {
    {
        .name       = "WinHttpConnect",
        .patch      = hook_WinHttpConnect,
        .link       = (void **) &next_WinHttpConnect,
    }, {
        .name       = "WinHttpCrackUrl",
        .patch      = hook_WinHttpCrackUrl,
        .link       = (void **) &next_WinHttpCrackUrl,
    }
};

static struct hook_symbol http_hook_syms_ws2[] = {
    {
        .name       = "send",
        .patch      = hook_send,
        .link       = (void **) &next_send
    },
};

static struct hook_symbol port_hook_syms_ws2[] = {
    {
        .name       = "connect",
        .patch      = hook_connect,
        .link       = (void **) &next_connect
    },
};

static bool dns_hook_initted;
static CRITICAL_SECTION dns_hook_lock;
static struct dns_hook_entry *dns_hook_entries;
static size_t dns_hook_nentries;
static char received_title_url[255];
static unsigned short startup_port;
static unsigned short billing_port;
static unsigned short aimedb_port;

static void dns_hook_init(void)
{
    if (dns_hook_initted) {
        return;
    }

    dns_hook_initted = true;
    InitializeCriticalSection(&dns_hook_lock);

    dns_hook_apply_hooks(NULL);
}

void dns_hook_apply_hooks(HMODULE mod){
    hook_table_apply(
            mod,
            "dnsapi.dll",
            dns_hook_syms_dnsapi,
            _countof(dns_hook_syms_dnsapi));

    hook_table_apply(
            mod,
            "ws2_32.dll",
            dns_hook_syms_ws2,
            _countof(dns_hook_syms_ws2));

    hook_table_apply(
            mod,
            "winhttp.dll",
            dns_hook_syms_winhttp,
            _countof(dns_hook_syms_winhttp));
}

void http_hook_init(){
    for (size_t i = 0; i < _countof(http_hook_syms_ws2); ++i) {
        http_hook_syms_ws2[i].ordinal = get_function_ordinal("ws2_32.dll", http_hook_syms_ws2[i].name);
    }

    hook_table_apply(
            NULL,
            "ws2_32.dll",
            http_hook_syms_ws2,
            _countof(http_hook_syms_ws2));
}

void port_hook_init(unsigned short _startup_port, unsigned short _billing_port, unsigned short _aimedb_port){
    startup_port = _startup_port;
    billing_port = _billing_port;
    aimedb_port = _aimedb_port;
    for (size_t i = 0; i < _countof(port_hook_syms_ws2); ++i) {
        port_hook_syms_ws2[i].ordinal = get_function_ordinal("ws2_32.dll", port_hook_syms_ws2[i].name);
    }

    hook_table_apply(
            NULL,
            "ws2_32.dll",
            port_hook_syms_ws2,
            _countof(port_hook_syms_ws2));
}

// This function match domain and subdomains like *.naominet.jp.
bool match_domain(const wchar_t* target, const wchar_t* pattern) {
    if (_wcsicmp(pattern, target) == 0) {
        return true;
    }

    int pattern_ptr_index = 0;
    int target_ptr_index = 0;

    while (pattern[pattern_ptr_index] != '\0' && target[target_ptr_index] != '\0') {
        if (pattern[pattern_ptr_index] == '*') {
            pattern_ptr_index++; // Check next character for wildcard match.

            while (pattern[pattern_ptr_index] != target[target_ptr_index]) {
                target_ptr_index++;

                if (target[target_ptr_index] == '\0') return false;
            }
        }
        else if (pattern[pattern_ptr_index] != target[target_ptr_index]) {
            return false;
        }
        else {
            pattern_ptr_index++;
            target_ptr_index++;
        }
    }

    return pattern[pattern_ptr_index] == '\0' && target[target_ptr_index] == '\0';
}

HRESULT dns_hook_push(const wchar_t *from_src, const wchar_t *to_src)
{
    HRESULT hr;
    struct dns_hook_entry *newmem;
    struct dns_hook_entry *newitem;
    wchar_t *from;
    wchar_t *to;

    assert(from_src != NULL);

    to = NULL;
    from = NULL;
    dns_hook_init();

    EnterCriticalSection(&dns_hook_lock);

    from = _wcsdup(from_src);

    if (from == NULL) {
        hr = E_OUTOFMEMORY;

        goto end;
    }

    if (to_src != NULL) {
        to = _wcsdup(to_src);

        if (to == NULL) {
            hr = E_OUTOFMEMORY;

            goto end;
        }
    }

    newmem = realloc(
            dns_hook_entries,
            (dns_hook_nentries + 1) * sizeof(struct dns_hook_entry));

    if (newmem == NULL) {
        hr = E_OUTOFMEMORY;

        goto end;
    }

    dns_hook_entries = newmem;
    newitem = &newmem[dns_hook_nentries++];
    newitem->from = from;
    newitem->to = to;

    from = NULL;
    to = NULL;
    hr = S_OK;

end:
    LeaveCriticalSection(&dns_hook_lock);

    free(to);
    free(from);

    return hr;
}

static DNS_STATUS WINAPI hook_DnsQuery_A(
        const char *pszName,
        WORD wType,
        DWORD Options,
        void *pExtra,
        DNS_RECORD **ppQueryResults,
        void *pReserved)
{
    const struct dns_hook_entry *pos;
    size_t i;
    size_t wstr_c;
    wchar_t *wstr;
    size_t str_c;
    char *str;
    DNS_STATUS code;
    HRESULT hr;

    wstr = NULL;
    str = NULL;

    if (pszName == NULL) {
        hr = HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);

        goto end;
    }

    mbstowcs_s(&wstr_c, NULL, 0, pszName, 0);
    wstr = malloc(wstr_c * sizeof(wchar_t));

    if (wstr == NULL) {
        hr = HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);

        goto end;
    }

    mbstowcs_s(NULL, wstr, wstr_c, pszName, wstr_c - 1);
    EnterCriticalSection(&dns_hook_lock);

    for (i = 0 ; i < dns_hook_nentries ; i++) {
        pos = &dns_hook_entries[i];

        if (match_domain(wstr, pos->from)) {
            if (pos->to == NULL) {
                LeaveCriticalSection(&dns_hook_lock);
                hr = HRESULT_FROM_WIN32(DNS_ERROR_RCODE_NAME_ERROR);

                goto end;
            }

            wcstombs_s(&str_c, NULL, 0, pos->to, 0);
            str = malloc(str_c * sizeof(char));

            if (str == NULL) {
                LeaveCriticalSection(&dns_hook_lock);
                hr = HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);

                goto end;
            }

            wcstombs_s(NULL, str, str_c, pos->to, str_c - 1);
            pszName = str;

            break;
        }
    }

    LeaveCriticalSection(&dns_hook_lock);

    code = next_DnsQuery_A(
            pszName,
            wType,
            Options,
            pExtra,
            ppQueryResults,
            pReserved);

    hr = HRESULT_FROM_WIN32(code);

end:
    free(str);
    free(wstr);

    return hr_to_win32_error(hr);
}

static DNS_STATUS WINAPI hook_DnsQuery_W(
        const wchar_t *pszName,
        WORD wType,
        DWORD Options,
        void *pExtra,
        DNS_RECORD **ppQueryResults,
        void *pReserved)
{
    const struct dns_hook_entry *pos;
    size_t i;

    if (pszName == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    EnterCriticalSection(&dns_hook_lock);

    for (i = 0 ; i < dns_hook_nentries ; i++) {
        pos = &dns_hook_entries[i];

        if (match_domain(pszName, pos->from)) {
            if (pos->to == NULL) {
                LeaveCriticalSection(&dns_hook_lock);
                return HRESULT_FROM_WIN32(DNS_ERROR_RCODE_NAME_ERROR);
            }

            pszName = pos->to;

            break;
        }
    }

    LeaveCriticalSection(&dns_hook_lock);

    return next_DnsQuery_W(
            pszName,
            wType,
            Options,
            pExtra,
            ppQueryResults,
            pReserved);

}

static DNS_STATUS WINAPI hook_DnsQueryEx(
        POLYFILL_DNS_QUERY_REQUEST *pRequest,
        void *pQueryResults,
        void *pCancelHandle)
{
    const wchar_t *orig;
    const struct dns_hook_entry *pos;
    DNS_STATUS code;
    size_t i;

    if (pRequest == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    orig = pRequest->QueryName;
    EnterCriticalSection(&dns_hook_lock);

    for (i = 0 ; i < dns_hook_nentries ; i++) {
        pos = &dns_hook_entries[i];

        if (match_domain(pRequest->QueryName, pos->from)) {
            if (pos->to == NULL) {
                LeaveCriticalSection(&dns_hook_lock);
                return HRESULT_FROM_WIN32(DNS_ERROR_RCODE_NAME_ERROR);
            }

            pRequest->QueryName = pos->to;

            break;
        }
    }

    LeaveCriticalSection(&dns_hook_lock);

    code = next_DnsQueryEx(pRequest, pQueryResults, pCancelHandle);

    /* Caller might not appreciate QueryName changing under its feet. It is
       strongly implied by MSDN that a copy of *pRequest is taken by WINAPI,
       so we can change it back after the call has been issued with no ill
       effect... we hope.

       Hopefully the completion callback is issued from an APC or something
       (or otherwise happens after this returns) or we're in trouble. */

    pRequest->QueryName = orig;

    return code;
}

static int WSAAPI hook_getaddrinfo(
        const char *pNodeName,
        const char *pServiceName,
        const ADDRINFOA *pHints,
        ADDRINFOA **ppResult)
{
    const struct dns_hook_entry *pos;
    char *str;
    size_t str_c;
    wchar_t *wstr;
    size_t wstr_c;
    int result;
    size_t i;

    str = NULL;
    wstr = NULL;

    if (pNodeName == NULL) {
        result = WSA_INVALID_PARAMETER;

        goto end;
    }

    mbstowcs_s(&wstr_c, NULL, 0, pNodeName, 0);
    wstr = malloc(wstr_c * sizeof(wchar_t));

    if (wstr == NULL) {
        result = WSA_NOT_ENOUGH_MEMORY;

        goto end;
    }

    mbstowcs_s(NULL, wstr, wstr_c, pNodeName, wstr_c - 1);
    EnterCriticalSection(&dns_hook_lock);

    for (i = 0 ; i < dns_hook_nentries ; i++) {
        pos = &dns_hook_entries[i];

        if (match_domain(wstr, pos->from)) {
            if (pos->to == NULL) {
                LeaveCriticalSection(&dns_hook_lock);
                result = EAI_NONAME;

                goto end;
            }

            wcstombs_s(&str_c, NULL, 0, pos->to, 0);
            str = malloc(str_c * sizeof(char));

            if (str == NULL) {
                LeaveCriticalSection(&dns_hook_lock);
                result = WSA_NOT_ENOUGH_MEMORY;

                goto end;
            }

            wcstombs_s(NULL, str, str_c, pos->to, str_c - 1);
            pNodeName = str;

            break;
        }
    }

    LeaveCriticalSection(&dns_hook_lock);

    result = next_getaddrinfo(pNodeName, pServiceName, pHints, ppResult);

end:
    free(wstr);
    free(str);

    return result;
}

static int WSAAPI hook_GetAddrInfoW(
        const wchar_t *pNodeName,
        const wchar_t *pServiceName,
        const ADDRINFOW *pHints,
        ADDRINFOW **ppResult)
{
    const struct dns_hook_entry *pos;
    int result;
    size_t i;

    if (pNodeName == NULL) {
        result = WSA_INVALID_PARAMETER;

        goto end;
    }

    EnterCriticalSection(&dns_hook_lock);

    for (i = 0 ; i < dns_hook_nentries ; i++) {
        pos = &dns_hook_entries[i];

        if (match_domain(pNodeName, pos->from)) {
            if(pos->to == NULL) {
                LeaveCriticalSection(&dns_hook_lock);
                result = EAI_NONAME;

                goto end;
            }

            // dprintf("GetAddrInfoW: %ls -> %ls\n", pNodeName, pos->to);

            pNodeName = pos->to;

            break;
        }
    }

    LeaveCriticalSection(&dns_hook_lock);

    result = next_GetAddrInfoW(pNodeName, pServiceName, pHints, ppResult);

end:
    return result;
}

static int WSAAPI hook_GetAddrInfoExA(
        const char *pName,
        const char *pServiceName,
        DWORD dwNameSpace,
        LPGUID lpNspId,
        const ADDRINFOEXW *hints,
        PADDRINFOEXA *ppResult,
        struct timeval *timeout,
        LPOVERLAPPED lpOverlapped,
        LPLOOKUPSERVICE_COMPLETION_ROUTINE lpCompletionRoutine,
        LPHANDLE lpHandle)
{
    const struct dns_hook_entry *pos;
    char *str;
    size_t str_c;
    wchar_t *wstr;
    size_t wstr_c;
    int result;
    size_t i;

    str = NULL;
    wstr = NULL;

    if (pName == NULL) {
        result = WSA_INVALID_PARAMETER;

        goto end;
    }

    mbstowcs_s(&wstr_c, NULL, 0, pName, 0);
    wstr = malloc(wstr_c * sizeof(wchar_t));

    if (wstr == NULL) {
        result = WSA_NOT_ENOUGH_MEMORY;

        goto end;
    }

    mbstowcs_s(NULL, wstr, wstr_c, pName, wstr_c - 1);

    EnterCriticalSection(&dns_hook_lock);

    for (i = 0; i < dns_hook_nentries; i++) {
        pos = &dns_hook_entries[i];

        if (match_domain(wstr, pos->from)) {
            if (pos->to == NULL) {
                LeaveCriticalSection(&dns_hook_lock);
                result = WSAHOST_NOT_FOUND;
                goto end;
            }

            // dprintf("GetAddrInfoExA: %ls -> %ls\n", pName, pos->to);

            wcstombs_s(&str_c, NULL, 0, pos->to, 0);
            str = malloc(str_c * sizeof(char));

            if (str == NULL) {
                LeaveCriticalSection(&dns_hook_lock);
                result = WSA_NOT_ENOUGH_MEMORY;

                goto end;
            }

            wcstombs_s(NULL, str, str_c, pos->to, str_c - 1);
            pName = str;

            break;
        }
    }

    LeaveCriticalSection(&dns_hook_lock);

    result = next_GetAddrInfoExA(pName, pServiceName, dwNameSpace, lpNspId,
                                 hints, ppResult, timeout, lpOverlapped,
                                 lpCompletionRoutine, lpHandle);

end:
    free(wstr);
    free(str);
    
    return result;
}

static int WSAAPI hook_GetAddrInfoExW(
        const wchar_t *pName,
        const wchar_t *pServiceName,
        DWORD dwNameSpace,
        LPGUID lpNspId,
        const ADDRINFOEXW *hints,
        PADDRINFOEXW *ppResult,
        struct timeval *timeout,
        LPOVERLAPPED lpOverlapped,
        LPLOOKUPSERVICE_COMPLETION_ROUTINE lpCompletionRoutine,
        LPHANDLE lpHandle)
{
    const struct dns_hook_entry *pos;
    int result;
    size_t i;

    if (pName == NULL) {
        result = WSA_INVALID_PARAMETER;
        goto end;
    }

    EnterCriticalSection(&dns_hook_lock);

    for (i = 0; i < dns_hook_nentries; i++) {
        pos = &dns_hook_entries[i];

        if (match_domain(pName, pos->from)) {
            if (pos->to == NULL) {
                LeaveCriticalSection(&dns_hook_lock);
                result = WSAHOST_NOT_FOUND;
                goto end;
            }

            // dprintf("GetAddrInfoExW: %ls -> %ls\n", pName, pos->to);

            pName = pos->to;
            break;
        }
    }

    LeaveCriticalSection(&dns_hook_lock);

    result = next_GetAddrInfoExW(pName, pServiceName, dwNameSpace, lpNspId,
                                 hints, ppResult, timeout, lpOverlapped,
                                 lpCompletionRoutine, lpHandle);

end:
    return result;
}

static HINTERNET WINAPI hook_WinHttpConnect(
        HINTERNET hSession,
        const wchar_t *pwszServerName,
        INTERNET_PORT nServerPort,
        DWORD dwReserved)
{
    const struct dns_hook_entry *pos;
    size_t i;

    if (pwszServerName == NULL) {
        return NULL;
    }

    EnterCriticalSection(&dns_hook_lock);

    for (i = 0 ; i < dns_hook_nentries ; i++) {
        pos = &dns_hook_entries[i];

        if (match_domain(pwszServerName, pos->from)) {
            if (pos->to == NULL) {
                LeaveCriticalSection(&dns_hook_lock);
                return NULL;
            }

            pwszServerName = pos->to;

            break;
        }
    }

    LeaveCriticalSection(&dns_hook_lock);

    return next_WinHttpConnect(hSession, pwszServerName, nServerPort, dwReserved);
}

// Hook to replace CXB title url
static bool WINAPI hook_WinHttpCrackUrl(
        const wchar_t *pwszUrl,
        DWORD dwUrlLength,
        DWORD dwFlags,
        LPURL_COMPONENTS lpUrlComponents)
{
    const struct dns_hook_entry *pos;
    size_t i;

    EnterCriticalSection(&dns_hook_lock);

    for (i = 0 ; i < dns_hook_nentries ; i++) {
        pos = &dns_hook_entries[i];

        if (match_domain(pwszUrl, pos->from)) {
            wchar_t* toAddr = pos->to;
            wchar_t titleBuffer[255];

            if (wcscmp(toAddr, L"title") == 0) {
                size_t wstr_c;
                mbstowcs_s(&wstr_c, titleBuffer, 255, received_title_url, strlen(received_title_url));
                toAddr = titleBuffer;
            }

            bool result = next_WinHttpCrackUrl(
                toAddr,
                wcslen(toAddr),
                dwFlags,
                lpUrlComponents
            );
            LeaveCriticalSection(&dns_hook_lock);
            return result;
        }
    }

    LeaveCriticalSection(&dns_hook_lock);
    return next_WinHttpCrackUrl(
        pwszUrl,
        dwUrlLength,
        dwFlags,
        lpUrlComponents
    );
}

int WINAPI hook_connect(SOCKET s, const struct sockaddr *name, int namelen) {
    const struct sockaddr_in *n;
    struct sockaddr_in new_name;
    unsigned ip;
    unsigned short port, new_port;

    EnterCriticalSection(&dns_hook_lock);

    n = (const struct sockaddr_in *)name;
    ip = n->sin_addr.S_un.S_addr;
    if (WSANtohs(s, n->sin_port, &port)) return SOCKET_ERROR;

    if (port == 80 && startup_port) {
        new_port = startup_port;
    } else if (port == 8443 && billing_port) {
        new_port = billing_port;
    } else if (port == 22345 && aimedb_port) {
        new_port = aimedb_port;
    } else { // No match
        dprintf("TCP Connect: %u.%u.%u.%u:%hu\n", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff, port);

        LeaveCriticalSection(&dns_hook_lock);
        return next_connect(
            s,
            name,
            namelen
        );
    }

    // matched
    new_name = *n;
    if (WSAHtons(s, new_port, &new_name.sin_port)) return SOCKET_ERROR;

    dprintf("TCP Connect: %u.%u.%u.%u:%hu, mapped to port %hu\n", ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff, port, new_port);

    LeaveCriticalSection(&dns_hook_lock);
    return next_connect(
        s,
        (const struct sockaddr *)&new_name,
        sizeof(new_name)
    );
}

DWORD WINAPI hook_send(SOCKET s, const char* buf, int len, int flags) {
    if (strstr(buf, "HTTP/") != NULL) {
        char *new_buf = malloc(len + 1);
        if (new_buf == NULL) return SOCKET_ERROR;

        memcpy(new_buf, buf, len);
        new_buf[len] = '\0';

        char *host_start = strstr(new_buf, "Host: ");
        if (host_start != NULL) {
            char *host_end = strstr(host_start, "\r\n");
            if (host_end != NULL) {
                host_end += 2;
                int host_len = host_end - host_start;

                char *host_value_start = host_start + 6;
                char *host_value_end = strstr(host_value_start, "\r\n");
                if (host_value_end != NULL) {
                    int value_len = host_value_end - host_value_start;
                    char* host_value = (char*)malloc(value_len + 1);
                    strncpy(host_value, host_value_start, value_len);
                    host_value[value_len] = '\0';

                    for (struct dns_hook_entry *entry = dns_hook_entries; entry && entry->from; entry++) {
                        char from_value[256];
                        wcstombs(from_value, entry->from, sizeof(from_value));

                        if (strcmp(host_value, from_value) == 0) {
                            char to_value[256];
                            wcstombs(to_value, entry->to, sizeof(to_value));
                            snprintf(host_start, len - (host_start - new_buf), "Host: %s\r\n", to_value);
                            break;
                        }
                    }
                    free(host_value);
                }
                len = (int)strlen(new_buf);
            }
        }
        DWORD result = next_send(s, new_buf, len, flags);
        free(new_buf);
        return result;
    }

    return next_send(s, buf, len, flags);
}