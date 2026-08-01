// ReSharper disable CppParameterNeverUsed
// ReSharper disable CppParameterMayBeConstPtrOrRef
#include "printer_cx.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include <wtypes.h>

#include "imageutil.h"
#include "hook/procaddr.h"
#include "hook/table.h"
#include "util/dprintf.h"

#pragma region prototypes
static int WINAPI hook_CXCMD_Retransfer();

static bool WINAPI hook_CXCMD_CheckIfConnected(int* pSlotId, int* pId);

static int WINAPI hook_CXCMD_xImageOut();

static int WINAPI hook_CXCMD_MoveCard(int slotId, int id, int dest, int flip, int filmInit, int immed);

static int WINAPI hook_CXCMD_xWriteMagData();

static int WINAPI hook_CXCMD_SecurityPrint(int slotId, int id, int color, int bufferIndex, int immed);

static int WINAPI hook_CXCMD_ScanPrinterNext();

static int WINAPI hook_CXCMD_Print(int slotId, int id, int color, int bufferIndex, int immed);

static int WINAPI hook_CXCMD_RezeroUnit(int slotId, int id, int action);

static int WINAPI hook_CXCMD_WriteMagData();

static int WINAPI hook_CXCMD_LogSense(int iSlot, int iID, int iPage, uint8_t* pbyBuffer);

static int WINAPI hook_CXCMD_StandardInquiry(int iSlot, int iID, uint8_t* pbyBuffer);

static int WINAPI hook_CXCMD_ModeSense(int iSlot, int iID, int iPC, int iPage, uint8_t* pbyBuffer);

static int WINAPI hook_CXCMD_UpdateFirmware(int iSlot, int iID, char* pFile, int iDataID);

static int WINAPI hook_CXCMD_ModeSelect(int iSlot, int iID, int iSp, int iPage, uint8_t* pbyData);

static int WINAPI hook_CXCMD_GetPrintingStatus();

static int WINAPI hook_CXCMD_SendDiagnostic(int iSlot, int iID, int iTestMode, int iTestPatten, int iTestCount);

static int WINAPI hook_CXCMD_RetransferAndTurn(int slotId, int id, int immed);

static int WINAPI hook_CXCMD_LogSelect(int iSlot, int iID, int iMod);

static int WINAPI hook_CXCMD_ReadPosition(int slotId, int id, uint8_t* buffer);

static int WINAPI hook_CXCMD_SecurityLock();

static int WINAPI hook_CXCMD_SetPrintingStatus();

static int WINAPI hook_CXCMD_xReadISOMagData();

static int WINAPI hook_CXCMD_WriteISO3TrackMagData();

static int WINAPI hook_CXCMD_PasswordSet();

static int WINAPI hook_CXCMD_GetPrinterStatus();

static int WINAPI hook_CXCMD_WriteProjectCode();

static int WINAPI hook_CXCMD_LoadCard(int slotId, int id, int dest, int flip, int filmInit, int immed);

static int WINAPI hook_CXCMD_ReadMagData();

static int WINAPI hook_CXCMD_ScanPrinter(int* pSlotId, int* pId);

static int WINAPI hook_CXCMD_xWriteISOMagData();

static int WINAPI hook_CXCMD_ICControl();

static int WINAPI hook_CXCMD_ImageOut(int slotId, int id, uint8_t* plane, int length, int color, int bufferIndex);

static int WINAPI hook_CXCMD_ReadBuffer(int iSlot, int iID, int iMode, int iBufferID, uint8_t* pbyData, int iOffset,
                                           int iLength);

static int WINAPI hook_CXCMD_xReadMagData();

static int WINAPI hook_CXCMD_WriteBuffer(int iSlot, int iID, int iMode, int iBufferID, uint8_t* pbyData, int iOffset,
                                            int iLength);

static int WINAPI hook_CXCMD_DefineLUT(int slotId, int id, int color, int length, uint8_t* buffer);

static int WINAPI hook_CXCMD_ReadISO3TrackMagData();

static int WINAPI hook_CXCMD_TestUnitReady(int slotId, int id);

static int WINAPI hook_CXCMD_RetransferAndEject(int slotId, int id, int immed);

static bool WINAPI hook_Lut24_Exchange(const wchar_t* pFile, uint8_t* y, uint8_t* m, uint8_t* c, int sizeY, int sizeM,
                                          int sizeC);

static bool WINAPI hook_Wdata_create(uint8_t* pbyRdata, uint8_t* pbyGdata, uint8_t* pbyBdata, int iWidth,
                                        int iHeight, bool bPortrait, int iAlgorithim, uint8_t* pbyWdata);
#pragma endregion

#pragma region hooktables
static const struct hook_symbol hook_pcp_syms[] = {
    {
        .name = "CXCMD_Retransfer",
        .patch = hook_CXCMD_Retransfer,
        .ordinal = 19,
    },
    {
        .name = "CXCMD_CheckIfConnected",
        .patch = hook_CXCMD_CheckIfConnected,
        .ordinal = 1,
    },
    {
        .name = "CXCMD_xImageOut",
        .patch = hook_CXCMD_xImageOut,
        .ordinal = 36,
    },
    {
        .name = "CXCMD_MoveCard",
        .patch = hook_CXCMD_MoveCard,
        .ordinal = 12,
    },
    {
        .name = "CXCMD_xWriteMagData",
        .patch = hook_CXCMD_xWriteMagData,
        .ordinal = 40,
    },
    {
        .name = "CXCMD_SecurityPrint",
        .patch = hook_CXCMD_SecurityPrint,
        .ordinal = 26,
    },
    {
        .name = "CXCMD_ScanPrinterNext",
        .patch = hook_CXCMD_ScanPrinterNext,
        .ordinal = 24,
    },
    {
        .name = "CXCMD_Print",
        .patch = hook_CXCMD_Print,
        .ordinal = 14,
    },
    {
        .name = "CXCMD_RezeroUnit",
        .patch = hook_CXCMD_RezeroUnit,
        .ordinal = 22,
    },
    {
        .name = "CXCMD_WriteMagData",
        .patch = hook_CXCMD_WriteMagData,
        .ordinal = 34,
    },
    {
        .name = "CXCMD_LogSense",
        .patch = hook_CXCMD_LogSense,
        .ordinal = 9,
    },
    {
        .name = "CXCMD_StandardInquiry",
        .patch = hook_CXCMD_StandardInquiry,
        .ordinal = 29,
    },
    {
        .name = "CXCMD_ModeSense",
        .patch = hook_CXCMD_ModeSense,
        .ordinal = 11,
    },
    {
        .name = "CXCMD_UpdateFirmware",
        .patch = hook_CXCMD_UpdateFirmware,
        .ordinal = 31,
    },
    {
        .name = "CXCMD_ModeSelect",
        .patch = hook_CXCMD_ModeSelect,
        .ordinal = 10,
    },
    {
        .name = "CXCMD_GetPrintingStatus",
        .patch = hook_CXCMD_GetPrintingStatus,
        .ordinal = 4,
    },
    {
        .name = "CXCMD_SendDiagnostic",
        .patch = hook_CXCMD_SendDiagnostic,
        .ordinal = 27,
    },
    {
        .name = "CXCMD_RetransferAndTurn",
        .patch = hook_CXCMD_RetransferAndTurn,
        .ordinal = 21,
    },
    {
        .name = "CXCMD_LogSelect",
        .patch = hook_CXCMD_LogSelect,
        .ordinal = 8,
    },
    {
        .name = "CXCMD_ReadPosition",
        .patch = hook_CXCMD_ReadPosition,
        .ordinal = 18,
    },
    {
        .name = "CXCMD_SecurityLock",
        .patch = hook_CXCMD_SecurityLock,
        .ordinal = 25,
    },
    {
        .name = "CXCMD_SetPrintingStatus",
        .patch = hook_CXCMD_SetPrintingStatus,
        .ordinal = 28,
    },
    {
        .name = "CXCMD_xReadISOMagData",
        .patch = hook_CXCMD_xReadISOMagData,
        .ordinal = 37,
    },
    {
        .name = "CXCMD_WriteISO3TrackMagData",
        .patch = hook_CXCMD_WriteISO3TrackMagData,
        .ordinal = 33,
    },
    {
        .name = "CXCMD_PasswordSet",
        .patch = hook_CXCMD_PasswordSet,
        .ordinal = 13,
    },
    {
        .name = "CXCMD_GetPrinterStatus",
        .patch = hook_CXCMD_GetPrinterStatus,
        .ordinal = 3,
    },
    {
        .name = "CXCMD_WriteProjectCode",
        .patch = hook_CXCMD_WriteProjectCode,
        .ordinal = 35,
    },
    {
        .name = "CXCMD_LoadCard",
        .patch = hook_CXCMD_LoadCard,
        .ordinal = 7,
    },
    {
        .name = "CXCMD_ReadMagData",
        .patch = hook_CXCMD_ReadMagData,
        .ordinal = 17,
    },
    {
        .name = "CXCMD_ScanPrinter",
        .patch = hook_CXCMD_ScanPrinter,
        .ordinal = 23,
    },
    {
        .name = "CXCMD_xWriteISOMagData",
        .patch = hook_CXCMD_xWriteISOMagData,
        .ordinal = 39,
    },
    {
        .name = "CXCMD_ICControl",
        .patch = hook_CXCMD_ICControl,
        .ordinal = 5,
    },
    {
        .name = "CXCMD_ImageOut",
        .patch = hook_CXCMD_ImageOut,
        .ordinal = 6,
    },
    {
        .name = "CXCMD_ReadBuffer",
        .patch = hook_CXCMD_ReadBuffer,
        .ordinal = 15,
    },
    {
        .name = "CXCMD_xReadMagData",
        .patch = hook_CXCMD_xReadMagData,
        .ordinal = 38,
    },
    {
        .name = "CXCMD_WriteBuffer",
        .patch = hook_CXCMD_WriteBuffer,
        .ordinal = 32,
    },
    {
        .name = "CXCMD_DefineLUT",
        .patch = hook_CXCMD_DefineLUT,
        .ordinal = 2,
    },
    {
        .name = "CXCMD_ReadISO3TrackMagData",
        .patch = hook_CXCMD_ReadISO3TrackMagData,
        .ordinal = 16,
    },
    {
        .name = "CXCMD_TestUnitReady",
        .patch = hook_CXCMD_TestUnitReady,
        .ordinal = 30,
    },
    {
        .name = "CXCMD_RetransferAndEject",
        .patch = hook_CXCMD_RetransferAndEject,
        .ordinal = 20,
    },
};

static const struct hook_symbol hook_lut_syms[] = {
    {
        .name = "Lut24_Exchange",
        .patch = hook_Lut24_Exchange,
        .ordinal = 1,
    },
};

static const struct hook_symbol hook_wdata_syms[] = {
    {
        .name = "Wdata_create",
        .patch = hook_Wdata_create,
        .ordinal = 1,
    },
};
#pragma endregion

static void write_int(uint8_t* data, int index, int value) {
    data[index] = value >> 24;
    data[index + 1] = value >> 16;
    data[index + 2] = value >> 8;
    data[index + 3] = value;
}

static void write_short(uint8_t* data, int index, short value) {
    data[index] = value >> 8;
    data[index + 1] = value;
}

static struct printer_cx_config printer_config;
static wchar_t printer_out_path[MAX_PATH];
static struct printer_cx_data printer_data;

#define HEIGHT 664
#define WIDTH 1036
#define IMAGE_BUFFER_SIZE WIDTH * HEIGHT
static uint8_t* back_buffer = NULL;
static uint8_t* front_buffer = NULL;
static uint64_t current_card_id;

DWORD load_printer_data() {
    DWORD bytesRead = 0;
    HANDLE hSave = CreateFileW(printer_config.printer_data_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSave != INVALID_HANDLE_VALUE) {
        if (!ReadFile(hSave, &printer_data, sizeof(printer_data), &bytesRead, NULL)){
            CloseHandle(hSave);
            return GetLastError();
        }
        CloseHandle(hSave);
        if (bytesRead != sizeof(printer_data)){
            return -1;
        }
        if (printer_data.version != PRINTER_DATA_VERSION) {
            return -2;
        }
        return 0;
    } else {
        return GetLastError();
    }
}

DWORD save_printer_data() {
    DWORD bytesWritten = 0;
    HANDLE hSave = CreateFileW(printer_config.printer_data_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSave != NULL) {
        if (!WriteFile(hSave, &printer_data, sizeof(printer_data), &bytesWritten, NULL)){
            CloseHandle(hSave);
            dprintf("CX7000: Failed writing data: %lx\n", GetLastError());
            return GetLastError();
        }
        CloseHandle(hSave);
        return 0;
    } else {
        dprintf("CX7000: Failed opening data file for writing: %lx\n", GetLastError());
        return GetLastError();
    }
}

void printer_cx_hook_init(const struct printer_cx_config* cfg, HINSTANCE self) {
    assert(cfg != NULL);

    if (!cfg->enable) {
        return;
    }

    memcpy(&printer_config, cfg, sizeof(*cfg));
    printer_cx_hook_insert_hooks(NULL);

    CreateDirectoryW(cfg->printer_out_path, NULL);
    memcpy(printer_out_path, cfg->printer_out_path, MAX_PATH);

    if (load_printer_data() != 0) {
        memset(&printer_data, 0, sizeof(printer_data));
        printer_data.version = PRINTER_DATA_VERSION;
        if (save_printer_data() == 0) {
            dprintf("CX7000: Printer data initialized.\n");
        }
    }

    dprintf("CX7000: hook enabled.\n");
}

void printer_cx_hook_insert_hooks(HMODULE target) {
    hook_table_apply(target, "PCP21CT64.dll", hook_pcp_syms, _countof(hook_pcp_syms));
    hook_table_apply(target, "LUT24EXG64.dll", hook_lut_syms, _countof(hook_lut_syms));
    hook_table_apply(target, "WCREATE64.dll", hook_wdata_syms, _countof(hook_wdata_syms));

    /* Unity workaround */
    proc_addr_table_push(target, "PCP21CT64.dll", hook_pcp_syms, _countof(hook_pcp_syms));
    proc_addr_table_push(target, "LUT24EXG64.dll", hook_lut_syms, _countof(hook_lut_syms));
    proc_addr_table_push(target, "WCREATE64.dll", hook_wdata_syms, _countof(hook_wdata_syms));
}

static int WINAPI hook_CXCMD_Retransfer() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static bool WINAPI hook_CXCMD_CheckIfConnected(int* pSlotId, int* pId) {
    dprintf("CX7000: %s\n", __func__);
    return printer_config.enable;
}

static int WINAPI hook_CXCMD_xImageOut() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_MoveCard(int slotId, int id, int dest, int flip, int filmInit, int immed) {
    dprintf("CX7000: %s(%d, %d, %d, %d)\n", __func__, dest, flip, filmInit, immed);
    return CX_OK;
}

static int WINAPI hook_CXCMD_xWriteMagData() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_SecurityPrint(int slotId, int id, int color, int bufferIndex, int immed) {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_ScanPrinterNext() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_ImageOut(int slotId, int id, uint8_t* plane, int length, int color, int bufferIndex) {
    dprintf("CX7000: %s\n", __func__);

    assert(color >= 0 && color <= 3);
    assert(bufferIndex >= 0 && bufferIndex <= 1);
    assert(length == IMAGE_BUFFER_SIZE);

    // colorIndex: 0 = w, 1 = c, 2 = m, 3 = y
    // bufferIndex: 0 = back, 1 = front

    if (bufferIndex == 0) {
        if (back_buffer == NULL) {
            back_buffer = (uint8_t*) malloc(4 * IMAGE_BUFFER_SIZE);
        }
        memcpy(back_buffer + color * IMAGE_BUFFER_SIZE, plane, length);
    } else {
        if (front_buffer == NULL) {
            front_buffer = (uint8_t*) malloc(4 * IMAGE_BUFFER_SIZE);
        }
        memcpy(front_buffer + color * IMAGE_BUFFER_SIZE, plane, length);
    }

    return CX_OK;
}

static int WINAPI hook_CXCMD_Print(int slotId, int id, int color, int bufferIndex, int immed) {
    dprintf("CX7000: %s(%d, %d, %d)\n", __func__, color, bufferIndex, immed);

    assert(bufferIndex >= 0 && bufferIndex <= 1);

    SYSTEMTIME t;
    GetLocalTime(&t);

    // color: 1 = back, 3 = front
    // bufferIndex: 0 = back, 1 = front

    wchar_t dumpPath[MAX_PATH];
    uint8_t metadata[5];

    metadata[0] = current_card_id >> 32;
    write_int(metadata, 1, (int32_t)current_card_id);

    swprintf_s(
        dumpPath, MAX_PATH,
        L"%s\\CX7000_%04d%02d%02d_%02d%02d%02d_%s.bmp",
        printer_out_path, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, bufferIndex == 0 ? L"back" : L"front");

    // convert image from seperate CMY arrays to one RGB array
    int size = IMAGE_BUFFER_SIZE * 3;
    uint8_t* raw_image = (uint8_t*) malloc(size);
    for (int i = 0; i < IMAGE_BUFFER_SIZE; i++) {
        // 0 is "white" and we don't really care about that
        raw_image[i * 3] = 0xFF - *((bufferIndex == 0 ? back_buffer : front_buffer) + IMAGE_BUFFER_SIZE + i);
        raw_image[i * 3 + 1] = 0xFF - *((bufferIndex == 0 ? back_buffer : front_buffer) + 2 * IMAGE_BUFFER_SIZE + i);
        raw_image[i * 3 + 2] = 0xFF - *((bufferIndex == 0 ? back_buffer : front_buffer) + 3 * IMAGE_BUFFER_SIZE + i);
    }

    dprintf("CX7000: Saving %s image to %ls\n", bufferIndex == 0 ? "back" : "front", dumpPath);

    int ret = WriteDataToBitmapFile(dumpPath, 24, WIDTH, HEIGHT, raw_image, size, metadata, 5, false);

    free(raw_image);
    if (bufferIndex == 0) {
        free(back_buffer);
        back_buffer = NULL;
    } else {
        free(front_buffer);
        front_buffer = NULL;
    }

    if (ret < 0) {
        dprintf("CX7000: WriteDataToBitmapFile returned %d\n", ret);
        return CX_ERROR_FATAL_3301;
    }

    return CX_OK;
}

static int WINAPI hook_CXCMD_RezeroUnit(int slotId, int id, int action) {
    dprintf("CX7000: %s\n", __func__);
    return CX_OK;
}

static int WINAPI hook_CXCMD_WriteMagData() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_LogSense(int iSlot, int iID, int iPage, uint8_t* pbyBuffer) {
    dprintf("CX7000: %s\n", __func__);

    const int size = 108;
    memset(pbyBuffer, 0, size);


    if (iPage == 56) {
        dprintf("CX7000: MediumQuantity\n");

        write_int(pbyBuffer, 8, (int)printer_data.print_counter); // total count
        write_int(pbyBuffer, 16, 22); // free count
        write_int(pbyBuffer, 24, 33); // head count
        write_int(pbyBuffer, 32, (int)printer_data.print_counter_since_clean); // cleaning count
        write_int(pbyBuffer, 40, 55); // error count
        write_int(pbyBuffer, 48, 66); // cru cleaning count

        return CX_OK;
    } else if (iPage == 57) {
        dprintf("CX7000: Miscellaneous\n");

        write_int(pbyBuffer, 16, 234); // re transfer hr power on time
        write_int(pbyBuffer, 24, 456); // remedy hr power on time
        write_int(pbyBuffer, 40, 789); // unresettable re transfer hr power on time
        write_int(pbyBuffer, 48, 1023); // unresettable remedy hr power on time

        return CX_OK;
    }

    dprintf("CX7000: Unknown LogSense\n");
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_StandardInquiry(int iSlot, int iID, uint8_t* pbyBuffer) {
    const int size = 96;
    dprintf("CX7000: %s\n", __func__);

    memset(pbyBuffer, 0, size);
    memcpy(pbyBuffer + 32, printer_config.printer_firm_version, 8);
    memcpy(pbyBuffer + 50, printer_config.printer_camera_version, 8);
    memcpy(pbyBuffer + 71, printer_config.printer_config_version, 8);
    memcpy(pbyBuffer + 79, printer_config.printer_table_version, 8);
    //memcpy(pbyBuffer + 58, printer_config.thermal_head_info, 13); // unused

    return CX_OK;
}

static int WINAPI hook_CXCMD_ModeSense(int iSlot, int iID, int iPC, int iPage, uint8_t* pbyBuffer) {
    dprintf("CX7000: %s(%d, %d)\n", __func__, iPC, iPage);

    const int size = 104;
    memset(pbyBuffer, 0, size);

    if (iPC == 1 && iPage == 40) { // GetMediaInfo
        pbyBuffer[51] = 10; // film count (10=100%)
        pbyBuffer[52] = 50; // ink count (50=100%)
        return CX_OK;
    } else if (iPC == 1 && iPage == 35) { // ReadInkInfo
        pbyBuffer[6] = 0; // "b"
        write_short(pbyBuffer, 8, 50); // Remain
        return CX_OK;
    }

    dprintf("CX7000: Unknown ModeSense\n");
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_UpdateFirmware(int iSlot, int iID, char* pFile, int iDataID) {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // intentionally not implemented
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_ModeSelect(int iSlot, int iID, int iSp, int iPage, uint8_t* pbyData) {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_GetPrintingStatus() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_SendDiagnostic(int iSlot, int iID, int iTestMode, int iTestPatten, int iTestCount) {
    dprintf("CX7000: %s(%d, %d, %d)\n", __func__, iTestMode, iTestPatten, iTestCount);
    if (iTestMode == 19) {
        dprintf("CX7000: Printer Front Buttons Enabled: %d\n", iTestPatten);
        return CX_OK;
    } else if (iTestMode == 17) {
        dprintf("CX7000: Printer was cleaned (haha)\n");
        printer_data.print_counter_since_clean = 0;
        save_printer_data();
        return CX_OK;
    } else if (iTestMode == 18) {
        dprintf("CX7000: Transport Mode enabled\n");
        printer_data.is_transport = true;
        save_printer_data();
        return CX_OK;
    } else if (iTestMode == 20) {
        dprintf("CX7000: Transport Mode disabled\n");
        printer_data.is_transport = false;
        save_printer_data();
        return CX_OK;
    }

    dprintf("CX7000: Unknown SendDiagnostic\n");
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_RetransferAndTurn(int slotId, int id, int immed) {
    dprintf("CX7000: %s\n", __func__);
    return CX_OK;
}

static int WINAPI hook_CXCMD_LogSelect(int iSlot, int iID, int iMod) {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_ReadPosition(int slotId, int id, uint8_t* buffer) {
    dprintf("CX7000: %s\n", __func__);
    const int size = 8;

    memset(buffer, 0, size);
    buffer[0] = 1 << 2; // IsExist (0 means YES!)
    buffer[7] = 0; // position (of card; 0 = printer, 1 = "IR")

    return CX_OK;
}

static int WINAPI hook_CXCMD_SecurityLock() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_SetPrintingStatus() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_xReadISOMagData() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_WriteISO3TrackMagData() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_PasswordSet() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_GetPrinterStatus() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_WriteProjectCode() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_LoadCard(int slotId, int id, int dest, int flip, int filmInit, int immed) {
    dprintf("CX7000: %s(%d, %d, %d, %d)\n", __func__, dest, flip, filmInit, immed);
    return CX_OK;
}

static int WINAPI hook_CXCMD_ReadMagData() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_ScanPrinter(int* pSlotId, int* pId) {
    dprintf("CX7000: %s\n", __func__);

    if (!printer_config.enable) {
        return CX_ERROR_NOT_CONNECTED_6804;
    }

    *pSlotId = 1;
    *pId = 1;

    return CX_OK;
}

static int WINAPI hook_CXCMD_xWriteISOMagData() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_ICControl() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_ReadBuffer(int iSlot, int iID, int iMode, int iBufferID, uint8_t* pbyData, int iOffset,
                                           int iLength) {
    dprintf("CX7000: %s(%d, %d, %d)\n", __func__, iMode, iBufferID, iLength);

    memset(pbyData, 0, iLength);

    if (iMode == 2 && iBufferID == 87 && iLength == 10) {
        dprintf("CX7000: ReadCondition\n");

        pbyData[0] = printer_data.is_transport; // transport mode
        pbyData[1] = 0; // head white ink level, unused
        pbyData[2] = 0; // main white ink level, unused
        pbyData[3] = 0; // total white ink level, unused

        return CX_OK;
    } else if (iMode == 2 && iBufferID == 112 && iLength == 6) {
        dprintf("CX7000: GetMacAddress\n");

        pbyData[0] = 0x12; // displays in test menu, else ununused?
        pbyData[1] = 0x34;
        pbyData[2] = 0x56;
        pbyData[3] = 0x78;
        pbyData[4] = 0x9A;
        pbyData[5] = 0xBC;

        return CX_OK;
    } else if (iMode == 2 && iBufferID == 82 && iLength == 4) {
        dprintf("CX7000: GetCleaningWaitCount\n");

        // seemingly unused
        write_short(pbyData, 0, 0);

        return CX_OK;
    } else if (iMode == 2 && iBufferID == 85 && iLength == 10) {
        dprintf("CX7000: GetSensorInfo\n");

        pbyData[0] = 244; // retransfer heat roller thermistor; must be over 243
        pbyData[1] = 0; // main pwb thermistor; unused
        pbyData[2] = 0; // thermal head thermistor; unused
        pbyData[3] = 0; // heater cover thermistor; unused

        return CX_OK;
    } else if (iMode == 2 && iBufferID == 88 && iLength == 16) {
        dprintf("CX7000: ReadErrorStatus\n");

        pbyData[1] = 0; // is door open?
        //pbyData[2...] = // any of the error codes that fit in a byte (from CX_ERROR_FATAL_3301 to CX_ERROR_PRINT_INTERRUPT_6805_3)

        return CX_OK;
    } else if (iMode == 2 && iBufferID == 224 && iLength == 10) {
        dprintf("CX7000: ReadCode\n");

        printer_data.print_counter_since_clean++;
        current_card_id = ++printer_data.print_counter;
        dprintf("CX7000: Generated new card ID: %lld\n", current_card_id);

        save_printer_data();

        pbyData[0] = current_card_id >> 32; // MSB of card id
        write_int(pbyData, 1, (int32_t)current_card_id); // lower 4 bytes of card id
        pbyData[5] = 0x0; // Direction (1 = rotate image by 180 degrees)
        pbyData[6] = 0x1; // CheckCode (0 = error)

        return CX_OK;
    } else if (iMode == 2 && iBufferID == 144 && iLength == 260) {
        dprintf("CX7000: ReadErrorLog\n");

        write_int(pbyData, 0, 0); // LogCount
        /*for (int i = 0; false; i++) { // list of error ids
            write_int(pbyData, i + 4, 0);
        }*/

        return CX_OK;
    }

    dprintf("CX7000: Unknown ReadBuffer\n");
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_xReadMagData() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_WriteBuffer(int iSlot, int iID, int iMode, int iBufferID, uint8_t* pbyData, int iOffset, // NOLINT(*-non-const-parameter)
                                            int iLength) {
    dprintf("CX7000: %s(%d, %d, %d, %d)\n", __func__, iMode, iBufferID, iOffset, iLength);
    if (iMode == 2 && iBufferID == 82 && iLength == 4) {
        int val = pbyData[0] << 8 | pbyData[1];
        dprintf("CX7000: Set cleaning limit: %d\n", val);

        return CX_OK;
    }

    dprintf("CX7000: Unknown WriteBuffer\n");
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_DefineLUT(int slotId, int id, int color, int length, uint8_t* buffer) {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_ReadISO3TrackMagData() {
    dprintf("CX7000: %s\n", __func__);
    dprintf("CX7000: Unimplemented\n"); // unused
    return CX_ERROR_USB_COM_3201;
}

static int WINAPI hook_CXCMD_TestUnitReady(int slotId, int id) {
    dprintf("CX7000: %s\n", __func__);

    if (!printer_config.enable) {
        return CX_ERROR_NOT_CONNECTED_6804;
    }

    return CX_OK;
}

static int WINAPI hook_CXCMD_RetransferAndEject(int slotId, int id, int immed) {
    dprintf("CX7000: %s\n", __func__);
    return CX_OK;
}

static bool WINAPI hook_Lut24_Exchange(const wchar_t* pFile, uint8_t* y, uint8_t* m, uint8_t* c, int sizeY, int sizeM,
                                          int sizeC) {
    dprintf("CX7000: %s(%ls)\n", __func__, pFile);

    // stub?

    return true;
}

static bool WINAPI hook_Wdata_create(uint8_t* pbyRdata, uint8_t* pbyGdata, uint8_t* pbyBdata, int iWidth,
                                        int iHeight, bool bPortrait, int iAlgorithim, uint8_t* pbyWdata) {
    dprintf("CX7000: %s(%d, %d, %d)\n", __func__, iHeight, bPortrait, iAlgorithim);

    // stub?

    return true;
}