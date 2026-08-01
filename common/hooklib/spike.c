#include <windows.h>
#include <inttypes.h>
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <psapi.h>
#include "hook/pe.h"
#include "hooklib/spike.h"
#include "util/dprintf.h"


/* Spike functions. Their "style" is named after the libc function they bear
   the closest resemblance to. */

static void spike_fn_puts(const char *msg)
{
    char line[512];

    sprintf_s(line, _countof(line), "%s\n", msg);
    OutputDebugStringA(line);
}

static void spike_fn_fputs(const char *msg)
{
    OutputDebugStringA(msg);
}

static void spike_fn_printf(const char *fmt, ...)
{
    char line[512];
    va_list ap;

    va_start(ap, fmt);
    vsprintf_s(line, _countof(line), fmt, ap);
    strcat(line, "\n");
    OutputDebugStringA(line);
}

static void spike_fn_vprintf(
        const char *proc,
        int line_no,
        const char *fmt,
        va_list ap)
{
    char msg[512];
    char line[512];

    vsprintf_s(msg, _countof(msg), fmt, ap);
    sprintf_s(line, _countof(line), "%s:%i: %s", proc, line_no, msg);
    OutputDebugStringA(line);
}

static void spike_fn_vwprintf(
        const wchar_t *proc,
        int line_no,
        const wchar_t *fmt,
        va_list ap)
{
    wchar_t msg[512];
    wchar_t line[512];

    vswprintf_s(msg, _countof(msg), fmt, ap);
    swprintf_s(line, _countof(line), L"%s:%i: %s", proc, line_no, msg);
    OutputDebugStringW(line);
}

static void spike_fn_perror(
        int a1,
        int a2,
        int error,
        const char *file,
        int line_no,
        const char *msg)
{
    char line[512];

    sprintf_s(
            line,
            _countof(line),
            "%s:%i:%08x: %s\n",
            file,
            line_no,
            error,
            msg);

    OutputDebugStringA(line);
}

BOOL is_valid_rva(LPCWSTR module_name, uintptr_t rva) {
    HMODULE module_base = GetModuleHandleW(module_name);
    if (!module_base) {
        return false;
    }
    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)module_base;
    PIMAGE_NT_HEADERS nt_headers = (PIMAGE_NT_HEADERS)((uint8_t *)module_base + dos_header->e_lfanew);
    DWORD module_size = nt_headers->OptionalHeader.SizeOfImage;
    return (rva < module_size);
}

/* Spike inserters */

static void spike_insert_jmp(LPCWSTR module_name, uintptr_t rva, void *proc) {
    uint8_t *base = (uint8_t *)GetModuleHandleW(module_name);
    if ( !(is_valid_rva(module_name, rva))) {
        dprintf("spike: Invalid RVA 0x%llx for module %S\n", (unsigned long long)rva, module_name);
        return;
    }
    uint8_t *target = base + rva;
    uint8_t *func_ptr = (uint8_t *)proc;
    uintptr_t target_addr = (uintptr_t)target;
    uintptr_t func_addr = (uintptr_t)func_ptr;

    #if defined(_WIN64) || defined(__amd64__)
        uint64_t relativeOffset = (uint64_t)(func_addr - target_addr - 5);
        uint8_t absoluteJump[] = {0x48, 0xB8, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00, 0xFF, 0xE0};
        memcpy(absoluteJump + 2, &func_ptr, 8);
        pe_patch(target, absoluteJump, sizeof(absoluteJump));
    #else
        uint32_t jumpOffset = (uint32_t)(func_addr - target_addr - 5);
        uint8_t relativeJump[] = {0xE9, 0x00, 0x00, 0x00, 0x00};
        memcpy(relativeJump + 1, &jumpOffset, 4);
        pe_patch(target, relativeJump, sizeof(relativeJump));
    #endif
}

static void spike_insert_ptr(LPCWSTR module_name, uintptr_t rva, void *ptr) {
    uint8_t *base;
    uint8_t *target;
    if (!(is_valid_rva(module_name, rva))) {
        dprintf("spike: Invalid RVA 0x%llx for module %S\n", (unsigned long long)rva, module_name);
        return;
    }
    base = (uint8_t *)GetModuleHandleW(module_name);
    target = base + rva;

    pe_patch(target, &ptr, sizeof(ptr));
}

static void spike_insert_nop(LPCWSTR module_name, uintptr_t rva, size_t count) {
    uint8_t *base;
    uint8_t *target;
    uint8_t *value;
    if (!(is_valid_rva(module_name, rva))) {
        dprintf("spike: Invalid RVA 0x%llx for module %S\n", (unsigned long long)rva, module_name);
        return;
    }
    base = (uint8_t *)GetModuleHandleW(module_name);
    target = base + rva;
    value = (uint8_t *)malloc(count);
    if (value == NULL) {
        return;
    }
    memset(value, 0x90, count);

    HRESULT ret = pe_patch(target, value, count);
    free(value);
}

static void spike_insert_data(LPCWSTR module_name, uintptr_t rva, const wchar_t *patch_str) {
    uint8_t patch_data[32];
    int length = 0;
    int byte_count = wcslen(patch_str) / 2;
    uint8_t *base;
    uint8_t *target;
    if (!(is_valid_rva(module_name, rva))) {
        dprintf("spike: Invalid RVA 0x%llx for module %S\n", (unsigned long long)rva, module_name);
        return;
    }
    base = (uint8_t *)GetModuleHandleW(module_name);
    target = base + rva;
    for (int i = 0; i < byte_count; i++) {
        unsigned int temp;
        if (swscanf(patch_str + i * 2, L"%2x", &temp) == 1) {
            patch_data[i] = (unsigned char)temp;
            length++;
        } else {
            break;
        }
    }

    pe_patch(target, patch_data, length);
}

static void spike_insert_log_levels(uintptr_t rva, size_t count) {
    uint8_t *base;
    uint32_t *levels;
    size_t i;

    base = (uint8_t *) GetModuleHandleW(NULL);
    levels = (uint32_t *) (base + rva);

    for (i = 0 ; i < count ; i++) {
        levels[i] = 255;
    }
}

/* Config reader */

void spike_hook_init(const wchar_t *ini_file)
{
    wchar_t module[MAX_PATH];
    wchar_t path[MAX_PATH];
    const wchar_t *basename;
    const wchar_t *slash;

    assert(ini_file != NULL);

    /* Get the filename (strip path) of the host EXE */

    GetModuleFileNameW(NULL, module, _countof(module));
    slash = wcsrchr(module, L'\\');

    if (slash != NULL) {
        basename = slash + 1;
    } else {
        basename = module;
    }

    /* Check our INI file to see if any spikes are configured for this EXE.
       Normally we separate out config reading into a separate module... */

    GetPrivateProfileStringW(
            L"spike",
            basename,
            L"",
            path,
            _countof(path),
            ini_file);

    if (path[0] != L'\0') {
        dprintf("Spiking %S using config from %S\n", basename, path);
        spike_hook_read_config(basename, path);
    }
}

void spike_hook_read_config(const wchar_t *target, const wchar_t *spike_file) {
    int match;
    int count;
    unsigned long long rva;
    char line[256];
    wchar_t filename[MAX_PATH];
    wchar_t patch_data[64];
    char *ret;
    FILE *f;
    int current_rva_mode = 0; // 0 = dec, 1 = hex
    wchar_t new_rva_mode[16];

    f = _wfopen(spike_file, L"r");

    if (f == NULL) {
        dprintf("Error opening spike file %S\n", spike_file);

        return;
    }

    for (;;) {
        ret = fgets(line, sizeof(line), f);

        if (ret == NULL) {
            break;
        }

        if (line[0] == '#' || line[0] == '\r' || line[0] == '\n') {
            continue;
        }

        match = sscanf(line, "mode %16ls", new_rva_mode);

        if (match == 1) {
            if (wcscmp(new_rva_mode, L"hex") == 0) {
                current_rva_mode = 1;
            } else if (wcscmp(new_rva_mode, L"dec") == 0) {
                current_rva_mode = 0;
            }
        }

        match = sscanf(line, current_rva_mode ? "levels %llx %i" : "levels %lli %i", &rva, &count);

        if (match == 2) {
            spike_insert_log_levels((uintptr_t)rva, count);
        }

        match = sscanf(line, current_rva_mode ? "j_vprintf %llx" : "j_vprintf %lli", &rva);

        if (match == 1) {
            spike_insert_jmp(target, (uintptr_t)rva, spike_fn_vprintf);
        }

        match = sscanf(line, current_rva_mode ? "j_vwprintf %llx" : "j_vwprintf %lli", &rva);

        if (match == 1) {
            spike_insert_jmp(target, (uintptr_t)rva, spike_fn_vwprintf);
        }

        match = sscanf(line, current_rva_mode ? "j_printf %llx" : "j_printf %lli", &rva);

        if (match == 1) {
            spike_insert_jmp(target, (uintptr_t)rva, spike_fn_printf);
        }

        match = sscanf(line, current_rva_mode ? "j_puts %llx" : "j_puts %lli", &rva);

        if (match == 1) {
            spike_insert_jmp(target, (uintptr_t)rva, spike_fn_puts);
        }

        match = sscanf(line, current_rva_mode ? "j_perror %llx" : "j_perror %lli", &rva);

        if (match == 1) {
            spike_insert_jmp(target, (uintptr_t)rva, spike_fn_perror);
        }

        match = sscanf(line, current_rva_mode ? "c_fputs %llx" : "c_fputs %lli", &rva); /* c == "callback" */

        if (match == 1) {
            spike_insert_ptr(target, (uintptr_t)rva, spike_fn_fputs);
        }
        match = sscanf(line, current_rva_mode ? "patch_memory_nop %255ls %llx %i" : "patch_memory_nop %255ls %lli %i", filename, &rva, &count);
        if (match == 3 && (_wcsicmp(filename, target) == 0)) {
            spike_insert_nop((LPCWSTR)filename, (uintptr_t)rva, count);
        }
        match = sscanf(line, current_rva_mode ? "patch_memory_data %255ls %llx %64ls" : "patch_memory_data %255ls %lli %64ls", filename, &rva, patch_data);
        if (match == 3 && (_wcsicmp(filename, target) == 0)) {
            spike_insert_data((LPCWSTR)filename, (uintptr_t)rva, patch_data);
        }
    }

    dprintf("Spike insertion complete\n");
    fclose(f);
}
