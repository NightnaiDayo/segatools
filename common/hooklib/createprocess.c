#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <shellapi.h>

#include "hook/table.h"

#include "hooklib/createprocess.h"

#include "path.h"
#include "hook/procaddr.h"
#include "util/dprintf.h"

void createprocess_hook_init();
static BOOL WINAPI my_CreateProcessA(
    LPCSTR                lpApplicationName,
    LPSTR                 lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL                  bInheritHandles,
    DWORD                 dwCreationFlags,
    LPVOID                lpEnvironment,
    LPCSTR                lpCurrentDirectory,
    LPSTARTUPINFOA        lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation
);
BOOL my_CreateProcessW(
    LPCWSTR               lpApplicationName,
    LPWSTR                lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL                  bInheritHandles,
    DWORD                 dwCreationFlags,
    LPVOID                lpEnvironment,
    LPCWSTR               lpCurrentDirectory,
    LPSTARTUPINFOW        lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation
    );

BOOL my_ShellExecuteExA(SHELLEXECUTEINFOA *pExecInfo);

BOOL my_ShellExecuteExW(SHELLEXECUTEINFOW *pExecInfo);

static BOOL (WINAPI *next_ShellExecuteExA)(SHELLEXECUTEINFOA *pExecInfo);

static BOOL (WINAPI *next_ShellExecuteExW)(SHELLEXECUTEINFOW *pExecInfo);

static BOOL (WINAPI *next_CreateProcessA)(
    LPCSTR                lpApplicationName,
    LPSTR                 lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL                  bInheritHandles,
    DWORD                 dwCreationFlags,
    LPVOID                lpEnvironment,
    LPCSTR                lpCurrentDirectory,
    LPSTARTUPINFOA        lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation
);

static BOOL (WINAPI *next_CreateProcessW)(
    LPCWSTR               lpApplicationName,
    LPWSTR                lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL                  bInheritHandles,
    DWORD                 dwCreationFlags,
    LPVOID                lpEnvironment,
    LPCWSTR               lpCurrentDirectory,
    LPSTARTUPINFOW        lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation
);

static const struct hook_symbol win32_hooks[] = {
    {
        .name = "CreateProcessA",
        .patch = my_CreateProcessA,
        .link = (void **) &next_CreateProcessA
    },
    {
        .name = "CreateProcessW",
        .patch = my_CreateProcessW,
        .link = (void **) &next_CreateProcessW
    },
};
static const struct hook_symbol shell32_hooks[] = {
    {
        .name = "ShellExecuteExA",
        .patch = my_ShellExecuteExA,
        .link = (void **) &next_ShellExecuteExA
    },
    {
        .name = "ShellExecuteExW",
        .patch = my_ShellExecuteExW,
        .link = (void **) &next_ShellExecuteExW
    },
};

static bool did_init = false;

static struct process_hook_sym_w *process_syms_w;
static struct process_hook_sym_a *process_syms_a;

static size_t process_nsyms_a = 0;
static size_t process_nsyms_w = 0;

static CRITICAL_SECTION createproc_lock;

HRESULT createprocess_push_hook_w(const wchar_t *name, const wchar_t *head, const wchar_t *tail, bool replace_all, bool replace_paths) {
    struct process_hook_sym_w *new_mem;
    struct process_hook_sym_w *new_proc;
    HRESULT hr;

    assert(name != NULL);
    assert(head != NULL);

    createprocess_hook_init();
    EnterCriticalSection(&createproc_lock);
    
    new_mem = realloc(
            process_syms_w,
            (process_nsyms_w + 1) * sizeof(struct process_hook_sym_w));

    if (new_mem == NULL) {

        LeaveCriticalSection(&createproc_lock);
        return E_OUTOFMEMORY;
    }

    new_proc = &new_mem[process_nsyms_w];
    memset(new_proc, 0, sizeof(*new_proc));
    new_proc->name = name;
    new_proc->head = head;
    new_proc->tail = tail;
    new_proc->replace_all = replace_all;
    new_proc->replace_paths = replace_paths;

    process_syms_w = new_mem;
    process_nsyms_w++;
    
    LeaveCriticalSection(&createproc_lock);
    return S_OK;
}

HRESULT createprocess_push_hook_a(const char *name, const char *head, const char *tail, bool replace_all, bool replace_paths) {
    struct process_hook_sym_a *new_mem;
    struct process_hook_sym_a *new_proc;

    assert(name != NULL);
    assert(head != NULL);

    createprocess_hook_init();

    EnterCriticalSection(&createproc_lock);
    
    new_mem = realloc(
            process_syms_a,
            (process_nsyms_a + 1) * sizeof(struct process_hook_sym_a));

    if (new_mem == NULL) {

        LeaveCriticalSection(&createproc_lock);
        return E_OUTOFMEMORY;
    }

    new_proc = &new_mem[process_nsyms_a];
    memset(new_proc, 0, sizeof(*new_proc));
    new_proc->name = name;
    new_proc->head = head;
    new_proc->tail = tail;
    new_proc->replace_all = replace_all;
    new_proc->replace_paths = replace_paths;

    process_syms_a = new_mem;
    process_nsyms_a++;
    
    LeaveCriticalSection(&createproc_lock);
    return S_OK;
}

void createprocess_hook_apply_hooks(HMODULE mod) {
    hook_table_apply(
            mod,
            "kernel32.dll",
            win32_hooks,
            _countof(win32_hooks));
    hook_table_apply(
            mod,
            "shell32.dll",
            shell32_hooks,
            _countof(shell32_hooks));

    proc_addr_table_push(mod, "kernel32.dll", win32_hooks, _countof(win32_hooks));
    proc_addr_table_push(mod, "shell32.dll", shell32_hooks, _countof(shell32_hooks));

    InitializeCriticalSection(&createproc_lock);
}

void createprocess_hook_init() {
    if (did_init) {
        return;
    }
    did_init = true;

    createprocess_hook_apply_hooks(NULL);
    dprintf("CreateProcess: Init\n");
}


static BOOL WINAPI my_CreateProcessA(
    LPCSTR                lpApplicationName,
    LPSTR                 lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL                  bInheritHandles,
    DWORD                 dwCreationFlags,
    LPVOID                lpEnvironment,
    LPCSTR                lpCurrentDirectory,
    LPSTARTUPINFOA        lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation
)
{
    for (int i = 0; i < process_nsyms_a; i++) {
        if (strncmp(process_syms_a[i].name, lpCommandLine, strlen(process_syms_a[i].name))) {
            continue;
        }

        dprintf("CreateProcess: Hooking child process %s %s\n", lpApplicationName, lpCommandLine);
        char new_cmd[MAX_PATH] = {0};
        strcat_s(new_cmd, MAX_PATH, process_syms_a[i].head);

        if (!process_syms_a[i].replace_all) {            
            strcat_s(new_cmd, MAX_PATH, lpCommandLine);
        }

        if (process_syms_a[i].tail != NULL) {
            strcat_s(new_cmd, MAX_PATH, process_syms_a[i].tail);
        }

        dprintf("CreateProcess: Replaced CreateProcessA %s\n", new_cmd);
        return next_CreateProcessA(
                lpApplicationName,
                new_cmd,
                lpProcessAttributes,
                lpThreadAttributes,
                bInheritHandles,
                dwCreationFlags,
                lpEnvironment,
                lpCurrentDirectory,
                lpStartupInfo,
                lpProcessInformation
            );
    }
    return next_CreateProcessA(
                lpApplicationName,
                lpCommandLine,
                lpProcessAttributes,
                lpThreadAttributes,
                bInheritHandles,
                dwCreationFlags,
                lpEnvironment,
                lpCurrentDirectory,
                lpStartupInfo,
                lpProcessInformation
            );
}

BOOL my_CreateProcessW(
    LPCWSTR               lpApplicationName,
    LPWSTR                lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL                  bInheritHandles,
    DWORD                 dwCreationFlags,
    LPVOID                lpEnvironment,
    LPCWSTR               lpCurrentDirectory,
    LPSTARTUPINFOW        lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation)
{
    return next_CreateProcessW(
        lpApplicationName,
        lpCommandLine,
        lpProcessAttributes,
        lpThreadAttributes,
        bInheritHandles,
        dwCreationFlags,
        lpEnvironment,
        lpCurrentDirectory,
        lpStartupInfo,
        lpProcessInformation
    );
}

BOOL my_ShellExecuteExA(SHELLEXECUTEINFOA *pExecInfo) {
    for (int i = 0; i < process_nsyms_a; i++) {
        if (strncmp(process_syms_a[i].name, pExecInfo->lpFile, strlen(process_syms_a[i].name))) {
            continue;
        }

        dprintf("CreateProcess: Hooking child process %s %s\n", pExecInfo->lpFile, pExecInfo->lpParameters);
        char new_args[MAX_PATH] = {0};
        strcat_s(new_args, MAX_PATH, process_syms_a[i].head);

        if (!process_syms_a[i].replace_all) {
            strcat_s(new_args, MAX_PATH, pExecInfo->lpParameters);
        }
        if (process_syms_a[i].replace_paths) {
            char result[MAX_PATH];
            if (path_transform_args_a(pExecInfo->lpParameters, ' ', result, MAX_PATH)) {
                strcat_s(new_args, MAX_PATH, result);
            }
        }

        if (process_syms_a[i].tail != NULL) {
            strcat_s(new_args, MAX_PATH, process_syms_a[i].tail);
        }

        pExecInfo->lpParameters = new_args;

        dprintf("CreateProcess: Replaced ShellExecuteExA %s %s\n", pExecInfo->lpFile, new_args);
    }
    return next_ShellExecuteExA(pExecInfo);
}

BOOL my_ShellExecuteExW(SHELLEXECUTEINFOW *pExecInfo) {
    for (int i = 0; i < process_nsyms_w; i++) {
        if (wcsncmp(process_syms_w[i].name, pExecInfo->lpFile, wcslen(process_syms_w[i].name))) {
            continue;
        }

        dprintf("CreateProcess: Hooking child process %ls %ls\n", pExecInfo->lpFile, pExecInfo->lpParameters);
        wchar_t new_args[MAX_PATH] = {0};
        wcscat_s(new_args, MAX_PATH, process_syms_w[i].head);

        if (!process_syms_w[i].replace_all) {
            wcscat_s(new_args, MAX_PATH, pExecInfo->lpParameters);
        }
        if (process_syms_w[i].replace_paths) {
            wchar_t result[MAX_PATH];
            if (path_transform_args_w(pExecInfo->lpParameters, ' ', result, MAX_PATH)) {
                wcscat_s(new_args, MAX_PATH, result);
            }
        }

        if (process_syms_w[i].tail != NULL) {
            wcscat_s(new_args, MAX_PATH, process_syms_w[i].tail);
        }

        pExecInfo->lpParameters = new_args;

        dprintf("CreateProcess: Replaced ShellExecuteExW %ls %ls\n", pExecInfo->lpFile, new_args);
    }
    return next_ShellExecuteExW(pExecInfo);
}