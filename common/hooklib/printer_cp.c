/*
 * MITSUBISHI CP9550DS printer basic emulator, used only in Project Diva.
 * I only analyzed the bare minimum to get this to work, as it's kinda a
 * feature nobody really cares about.
 */

// ReSharper disable CppParameterNeverUsed
// ReSharper disable CppParameterMayBeConstPtrOrRef
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "printer_cp.h"

#include "imageutil.h"
#include "hook/table.h"
#include "util/dprintf.h"

static int WINAPI hook_CPU9CheckPrinter(char* result, void* a2);
static int WINAPI hook_CPUXSearchPrinters(char* data, int countRequested, void* a3, int* connectedPrinterCount);
static int WINAPI hook_CPUASendImage(struct printer_cp_image* image, void* a2, void* a3, void* a4);
static int WINAPI hook_CPUXInit2();
static int WINAPI hook_CPU9CheckPrintEnd(int a1, bool* printEnd, void* a3);
static int WINAPI hook_CPU9CheckPaperRemain(char* result, void* a2);
static int WINAPI hook_CPU9GetFWInfo(char* firmware, void* a2);
static int WINAPI hook_CPU9PrintOut(void* a1, void* a2);
static int WINAPI hook_CPUXInit();
static int WINAPI hook_CPUASetPrintParameter(void* a1, void* a2);

static const struct hook_symbol printer_syms[] = {
    {
        .name  = "CPU9CheckPrinter",
        .patch = hook_CPU9CheckPrinter,
    },
    {
        .name  = "CPUXSearchPrinters",
        .patch = hook_CPUXSearchPrinters,
    },
    {
        .name  = "CPUASendImage",
        .patch = hook_CPUASendImage,
    },
    {
        .name  = "CPUXInit2",
        .patch = hook_CPUXInit2,
    },
    {
        .name  = "CPU9CheckPrintEnd",
        .patch = hook_CPU9CheckPrintEnd,
    },
    {
        .name  = "CPU9CheckPaperRemain",
        .patch = hook_CPU9CheckPaperRemain,
    },
    {
        .name  = "CPU9GetFWInfo",
        .patch = hook_CPU9GetFWInfo,
    },
    {
        .name  = "CPU9PrintOut",
        .patch = hook_CPU9PrintOut,
    },
    {
        .name  = "CPUXInit",
        .patch = hook_CPUXInit,
    },
    {
        .name  = "CPUASetPrintParameter",
        .patch = hook_CPUASetPrintParameter,
    },
};

static struct printer_cp_config printer_config;
static ULONGLONG print_start_time = 0;

HRESULT printer_cp_hook_init(const struct printer_cp_config* cfg, HINSTANCE self) {
    assert(cfg != NULL);

    if (!cfg->enable) {
        return S_FALSE;
    }

    memcpy(&printer_config, cfg, sizeof(*cfg));

    printer_cp_hook_insert_hooks(NULL);
    printer_cp_hook_insert_hooks(self);

    if (!CreateDirectoryW(cfg->printer_out_path, NULL)) {
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
            dprintf("CP9550DS: Failed to create directory: %lx\n", GetLastError());
            return HRESULT_FROM_WIN32(GetLastError());
        }
    }

    return S_OK;
}

void printer_cp_hook_insert_hooks(HMODULE target) {
    hook_table_apply(target, "CPUSBSG.dll", printer_syms, _countof(printer_syms));
}

static int WINAPI hook_CPU9CheckPrinter(char* result, void* a2) {
    dprintf("CP9550DS: %s\n", __func__);

    // this can be null!?
    if (result != NULL) {
        memset(result, 0, 8);
        result[0] = PRINTER_STATUS_READY;
    }

    return PRINTER_ERROR_NONE;
}

static int WINAPI hook_CPUXSearchPrinters(char* data, int countRequested, void* a3, int* connectedPrinterCount) {
    dprintf("CP9550DS: %s(%d)\n", __func__, countRequested);

    *connectedPrinterCount = 1;

    if (countRequested == 0) {
        // I guess this is requesting a buffer size?
        // This doesn't seem to happen always.
        dprintf("CP9550DS: countRequested = 0?\n");
        return PRINTER_ERROR_105;
    }

    // unknown
    data[0] = 0x01;
    data[1] = 0x01;
    data[2] = 0x01;
    data[3] = 0x01;
    // serial
    strcpy(data + 4, printer_config.serial_no);
    // unknown
    data[10] = 0x01;
    data[11] = 0x01;

    return PRINTER_ERROR_NONE;
}

static int WINAPI hook_CPUASendImage(struct printer_cp_image* image, void* a2, void* a3, void* a4) {
    dprintf("CP9550DS: %s(%d, %d, %d, %d, %d)\n", __func__, image->unk1, image->unk2, image->unk3, image->width, image->height);

    wchar_t dumpPath[MAX_PATH];

    SYSTEMTIME t;
    GetLocalTime(&t);

    swprintf_s(
        dumpPath, MAX_PATH,
        L"%s\\CP9550DS_%04d%02d%02d_%02d%02d%02d.bmp",
        printer_config.printer_out_path, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    
    int size = image->width * image->height * 3;
    uint8_t* raw_image = (uint8_t*) malloc(size);

    if (raw_image == NULL) {
        dprintf("CP9550DS: Memory allocation failed\n");
        return PRINTER_ERROR_GENERIC;
    }

    for (int i = 0; i < image->width * image->height; i++) {
        // flip red and blue channels
        raw_image[i * 3] = *(image->data + i * 3 + 2);
        raw_image[i * 3 + 1] = *(image->data + i * 3 + 1);
        raw_image[i * 3 + 2] = *(image->data + i * 3);
    }

    dprintf("CP9550DS: Saving image to %ls\n", dumpPath);

    int ret = WriteDataToBitmapFile(dumpPath, 24, image->width, image->height, raw_image, size, NULL, 0, false);

    free(raw_image);

    if (ret < 0) {
        dprintf("CP9550DS: WriteDataToBitmapFile returned %d\n", ret);
        return PRINTER_ERROR_GENERIC;
    }

    print_start_time = GetTickCount64();

    return PRINTER_ERROR_NONE;
}

static int WINAPI hook_CPUXInit2() {
    dprintf("CP9550DS: %s\n", __func__);

    return PRINTER_ERROR_NONE;
}

static int WINAPI hook_CPU9CheckPrintEnd(int a1, bool* printEnd, void* a3) {

    *printEnd = GetTickCount64() - print_start_time > printer_config.print_time * 1000;

    return PRINTER_ERROR_NONE;
}

static int WINAPI hook_CPU9CheckPaperRemain(char* result, void* a2) {
    dprintf("CP9550DS: %s\n", __func__);

    // paper count
    result[0] = 0xFF;

    return PRINTER_ERROR_NONE;
}

static int WINAPI hook_CPU9GetFWInfo(char* firmware, void* a2) {
    dprintf("CP9550DS: %s\n", __func__);

    // this doesn't seem to be checked anywhere. shows in test menu though.
    // this is also a string??
    memset(firmware, 0, 18);
    strcpy(firmware, "293A30");
    strcpy(firmware + 6, "378B10");
    strcpy(firmware + 12, "170A20");

    return PRINTER_ERROR_NONE;
}

static int WINAPI hook_CPU9PrintOut(void* a1, void* a2) {
    dprintf("CP9550DS: %s\n", __func__);

    return PRINTER_ERROR_NONE;
}

static int WINAPI hook_CPUXInit() {
    dprintf("CP9550DS: %s\n", __func__);

    return PRINTER_ERROR_NONE;
}

static int WINAPI hook_CPUASetPrintParameter(void* a1, void* a2) {
    dprintf("CP9550DS: %s\n", __func__);

    return PRINTER_ERROR_NONE;
}
