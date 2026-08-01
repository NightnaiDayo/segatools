// ReSharper disable CppParameterNeverUsed
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "y3.h"

#include "hook/table.h"
#include "hook/procaddr.h"

#include "hooklib/y3-dll.h"

#include "util/dprintf.h"

#if _WIN32 || _WIN64
    #if _WIN64
        #define ENV64BIT
    #else
        #define ENV32BIT
    #endif
#endif

// Check GCC
#if __GNUC__
    #if __x86_64__ || __ppc64__
        #define ENV64BIT
    #else
        #define ENV32BIT
    #endif
#endif

#ifdef ENV64BIT
    #define CALL
#else
    #define CALL __cdecl
#endif

float CALL API_DLLVersion();
uint32_t CALL API_GetLastError(int* hDevice);
uint32_t CALL API_GetErrorMessage(uint32_t errNo, char* szMessage, int numBytes);
int* CALL API_Connect(char* szPortName);
int CALL API_Close(int* hDevice);
int CALL API_Start(int* hDevice);
int CALL API_Stop(int* hDevice);
float CALL API_GetFirmVersion(int* hDevice);
uint32_t CALL API_GetFirmName(int* hDevice);
uint32_t CALL API_GetTargetCode(int* hDevice);
uint32_t CALL API_GetStatus(int* hDevice);
uint32_t CALL API_GetCounter(int* hDevice);
int CALL API_ClearError(int* hDevice);
int CALL API_Reset(int* hDevice, bool isHardReset);
int CALL API_GetCardInfo(int* hDevice, int numCards, struct CardInfo* pCardInfo);
int CALL API_GetCardInfoCharSize();
int CALL API_FirmwareUpdate(int* hDevice, uint32_t address, uint32_t size, uint8_t* buffer);
int CALL API_Calibration(int* hDevice, int calib);
int CALL API_GetCalibrationResult(int* hDevice, int calib, uint32_t* result);
uint32_t CALL API_GetProcTime(int* hDevice);
uint32_t CALL API_GetMemStatus(int* hDevice);
uint32_t CALL API_GetMemCounter(int* hDevice);
int CALL API_SetParameter(int* hDevice, uint32_t uParam, uint32_t* pParam);
int CALL API_GetParameter(int* hDevice, uint32_t uParam, uint32_t* pParam);

signed int CALL API_SetDevice(int a1, int a2);
signed int CALL API_SetCommand(int a1, int a2, int a3, int* a4);
signed int CALL API_SetSysControl(int a1, int a2, int* a3);
signed int CALL API_GetSysControl(int a1, int a2, int* a3);
int CALL API_TestReset(int a1);
signed int API_DebugReset(int a1, ...);
int CALL API_GetBoardType(int a1);
int CALL API_GetCardDataSize(int a1);
int CALL API_GetFirmDate(int a1);
int API_SystemCommand(int a1, char a2, ...);
int CALL API_CalcCheckSum(DWORD* a1, int a2, int a3);
int CALL API_GetCheckSumResult(int a1);
int CALL API_BlockRead(int a1, int a2, int a3, SIZE_T dwBytes);
int CALL API_GetBlockReadResult(int a1, void* a2);
int CALL API_BlockWrite(int a1, int a2, int a3, SIZE_T dwBytes, void* a5);
signed int CALL API_GetDebugParam(int a1, int a2, DWORD* a3);

uint32_t convert_string_to_uint(const char* firmName);

static const struct hook_symbol Y3_hooks[] = {
    {
        .name   = "API_DLLVersion",
        .patch  = API_DLLVersion,
        .link   = NULL
    },
    {
        .name   = "API_GetLastError",
        .patch  = API_GetLastError,
        .link   = NULL
    },
    {
        .name   = "API_GetErrorMessage",
        .patch  = API_GetErrorMessage,
        .link   = NULL
    },
    {
        .name   = "API_Connect",
        .patch  = API_Connect,
        .link   = NULL
    },
    {
        .name   = "API_Close",
        .patch  = API_Close,
        .link   = NULL
    },
    {
        .name   = "API_Start",
        .patch  = API_Start,
        .link   = NULL
    },
    {
        .name   = "API_Stop",
        .patch  = API_Stop,
        .link   = NULL
    },
    {
        .name   = "API_GetFirmVersion",
        .patch  = API_GetFirmVersion,
        .link   = NULL
    },
    {
        .name   = "API_GetFirmName",
        .patch  = API_GetFirmName,
        .link   = NULL
    },
    {
        .name   = "API_GetTargetCode",
        .patch  = API_GetTargetCode,
        .link   = NULL
    },
    {
        .name   = "API_GetStatus",
        .patch  = API_GetStatus,
        .link   = NULL
    },
    {
        .name   = "API_GetCounter",
        .patch  = API_GetCounter,
        .link   = NULL
    },
    {
        .name   = "API_Reset",
        .patch  = API_Reset,
        .link   = NULL
    },
    {
        .name   = "API_GetCardInfo",
        .patch  = API_GetCardInfo,
        .link   = NULL
    },
    {
        .name   = "API_GetCardInfoCharSize",
        .patch  = API_GetCardInfoCharSize,
        .link   = NULL
    },
    {
        .name   = "API_FirmwareUpdate",
        .patch  = API_FirmwareUpdate,
        .link   = NULL
    },
    {
        .name   = "API_Calibration",
        .patch  = API_Calibration,
        .link   = NULL
    },
    {
        .name   = "API_GetCalibrationResult",
        .patch  = API_GetCalibrationResult,
        .link   = NULL
    },
    {
        .name   = "API_GetProcTime",
        .patch  = API_GetProcTime,
        .link   = NULL
    },
    {
        .name   = "API_GetMemStatus",
        .patch  = API_GetMemStatus,
        .link   = NULL
    },
    {
        .name   = "API_GetMemCounter",
        .patch  = API_GetMemCounter,
        .link   = NULL
    },
    {
        .name   = "API_SetParameter",
        .patch  = API_SetParameter,
        .link   = NULL
    },
    {
        .name   = "API_GetParameter",
        .patch  = API_GetParameter,
        .link   = NULL
    },
    {
        .name   = "API_SetDevice",
        .patch  = API_SetDevice,
        .link   = NULL
    },
    {
        .name   = "API_SetCommand",
        .patch  = API_SetCommand,
        .link   = NULL
    },
    {
        .name   = "API_SetSysControl",
        .patch  = API_SetSysControl,
        .link   = NULL
    },
    {
        .name   = "API_GetSysControl",
        .patch  = API_GetSysControl,
        .link   = NULL
    },
    {
        .name   = "API_TestReset",
        .patch  = API_TestReset,
        .link   = NULL
    },
    {
        .name   = "API_DebugReset",
        .patch  = API_DebugReset,
        .link   = NULL
    },
    {
        .name   = "API_GetBoardType",
        .patch  = API_GetBoardType,
        .link   = NULL
    },
    {
        .name   = "API_GetCardDataSize",
        .patch  = API_GetCardDataSize,
        .link   = NULL
    },
    {
        .name   = "API_GetFirmDate",
        .patch  = API_GetFirmDate,
        .link   = NULL
    },
    {
        .name   = "API_SystemCommand",
        .patch  = API_SystemCommand,
        .link   = NULL
    },
    {
        .name   = "API_CalcCheckSum",
        .patch  = API_CalcCheckSum,
        .link   = NULL
    },
    {
        .name   = "API_GetCheckSumResult",
        .patch  = API_GetCheckSumResult,
        .link   = NULL
    },
    {
        .name   = "API_BlockRead",
        .patch  = API_BlockRead,
        .link   = NULL
    },
    {
        .name   = "API_GetBlockReadResult",
        .patch  = API_GetBlockReadResult,
        .link   = NULL
    },
    {
        .name   = "API_BlockWrite",
        .patch  = API_BlockWrite,
        .link   = NULL
    },
    {
        .name   = "API_GetDebugParam",
        .patch  = API_GetDebugParam,
        .link   = NULL
    },
};

static struct y3_config y3_config;

#define MAX_CARD_SIZE 32
static struct CardInfo card_data[MAX_CARD_SIZE];

static int* Y3_COM_FIELD = (int*)10;
static int* Y3_COM_PRINT = (int*)11;

HRESULT y3_hook_init(const struct y3_config* cfg, HINSTANCE self, const wchar_t* config_filename) {
    HRESULT hr;
    assert(cfg != NULL);

    if (!cfg->enable) {
        return S_FALSE;
    }

    memcpy(&y3_config, cfg, sizeof(*cfg));
    Y3_COM_FIELD = (int*)(uintptr_t)cfg->port_field;
    Y3_COM_PRINT = (int*)(uintptr_t)cfg->port_printer;

    y3_insert_hooks(NULL);

    memset(card_data, 0, sizeof(card_data));

    struct y3_dll_config config;
    y3_dll_config_load(&config, config_filename);

    hr = y3_dll_init(&config, self);
    if (FAILED(hr)) {
        return hr;
    }

    dprintf("Y3: hook enabled (field port = %d, printer port = %d)\n", cfg->port_field, cfg->port_printer);

    return hr;
}

void y3_insert_hooks(HMODULE target) {
    hook_table_apply(
        target,
        "Y3CodeReaderNE.dll",
        Y3_hooks,
        _countof(Y3_hooks));

    proc_addr_table_push(
        target,
        "Y3CodeReaderNE.dll",
        Y3_hooks,
        _countof(Y3_hooks));
}

float CALL API_DLLVersion() {
    dprintf("Y3: %s\n", __func__);
    return 1;
}

uint32_t CALL API_GetLastError(int* hDevice) {
    dprintf("Y3: %s\n", __func__);
    if (!y3_config.enable) {
        return 1;
    }
    return 0;
}

uint32_t CALL API_GetErrorMessage(uint32_t errNo, char* szMessage,
                                  int numBytes) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int* CALL API_Connect(char* szPortName) {
    HRESULT hr;

    dprintf("Y3: %s(%s)\n", __func__, szPortName);

    if (!y3_config.enable) {
        return NULL;
    }

    char number[2];
    strncpy(number, szPortName + 3, 2);
    int* hDevice = (int*)(uintptr_t)atoi(number);

    if (hDevice == Y3_COM_FIELD) {
        hr = y3_dll.init();
        if (FAILED(hr)) {
            dprintf("Y3: Hook DLL initialization failed: %lx\n", hr);
            return NULL;
        }
    }

    return hDevice;
}

int CALL API_Close(int* hDevice) {
    dprintf("Y3: %s(%p)\n", __func__, hDevice);

    if (hDevice == Y3_COM_FIELD) {
        y3_dll.close();
    }

    return 0;
}

int CALL API_Start(int* hDevice) {
    dprintf("Y3: %s(%p)\n", __func__, hDevice);
    return 0;
}

int CALL API_Stop(int* hDevice) {
    dprintf("Y3: %s(%p)\n", __func__, hDevice);
    return 0;
}

float CALL API_GetFirmVersion(int* hDevice) {
    dprintf("Y3: %s(%p)\n", __func__, hDevice);
    return 1;
}

uint32_t CALL API_GetFirmName(int* hDevice) {
    uint32_t result = 0;
    dprintf("Y3: %s(%p)\n", __func__, hDevice);

    if (hDevice == Y3_COM_FIELD) {
        result = convert_string_to_uint(y3_config.firm_name_field);
        dprintf("Y3: This device is a FIELD: %s\n", y3_config.firm_name_field);
    } else if (hDevice == Y3_COM_PRINT) {
        result = convert_string_to_uint(y3_config.firm_name_printer);
        dprintf("Y3: This device is a PRINTER: %s\n", y3_config.firm_name_printer);
    } else {
        dprintf("Y3: This device is UNKNOWN\n");
    }

    return result;
}

uint32_t CALL API_GetTargetCode(int* hDevice) {
    uint32_t result = 1162760014;
    dprintf("Y3: %s(%p)\n", __func__, hDevice);

    if (hDevice == Y3_COM_FIELD) {
        result = convert_string_to_uint(y3_config.target_code_field);
        dprintf("Y3: This device is a FIELD: %s\n", y3_config.target_code_field);
    } else if (hDevice == Y3_COM_PRINT) {
        result = convert_string_to_uint(y3_config.target_code_printer);
        dprintf("Y3: This device is a PRINTER: %s\n", y3_config.target_code_printer);
    } else {
        dprintf("Y3: This Y3 device is UNKNOWN\n");
    }

    return result;
}

uint32_t CALL API_GetStatus(int* hDevice) {
    // dprintf("Y3: %s\n", __func__);
    return 0;
}

uint32_t CALL API_GetCounter(int* hDevice) {
    // dprintf("Y3: %s\n", __func__);
    return 0;
}

int CALL API_ClearError(int* hDevice) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int CALL API_Reset(int* hDevice, bool isHardReset) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int CALL API_GetCardInfo(int* hDevice, int numCards, struct CardInfo* pCardInfo) {
    // dprintf("Y3: %s(%p), %d\n", __func__, hDevice, numCards);
    //  ret = num cards
    //  numCards = max cards

    if (hDevice == Y3_COM_FIELD) {

        int cards = numCards;
        HRESULT hr = y3_dll.get_cards(pCardInfo, &cards);
        if (FAILED(hr)) {
            dprintf("Y3: DLL returned error when retrieving cards: %lx\n", hr);
            return 0;
        }

        return cards;
    } else if (hDevice == Y3_COM_PRINT) {
        pCardInfo[0].fX = 0;
        pCardInfo[0].fY = 0;
        pCardInfo[0].fAngle = 0;
        pCardInfo[0].eCardType = TYPE0;
        pCardInfo[0].eCardStatus = MARKER;
        pCardInfo[0].uID = 0x10;
        pCardInfo[0].nNumChars = 0;
        pCardInfo[0].ubChar0.Data = 0;
        pCardInfo[0].ubChar1.Data = 0x4000;
        pCardInfo[0].ubChar2.Data = 0;
        pCardInfo[0].ubChar3.Data = 0x0;  // 40
        pCardInfo[0].ubChar4.Data = 0;
        pCardInfo[0].ubChar5.Data = 0;
        return 1;
    }

    return 0;
}

int CALL API_GetCardInfoCharSize() {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int CALL API_FirmwareUpdate(int* hDevice, uint32_t address, uint32_t size,
                            uint8_t* buffer) {
    dprintf("Y3: %s\n", __func__);
    return 1;  // not supported
}

int CALL API_Calibration(int* hDevice, int calib) {
    dprintf("Y3: %s\n", __func__);
    return 1;
}

int CALL API_GetCalibrationResult(int* hDevice, int calib, uint32_t* result) {
    dprintf("Y3: %s\n", __func__);
    return 1;
}

uint32_t CALL API_GetProcTime(int* hDevice) {
    // dprintf("Y3: %s\n", __func__);
    return 0;
}

uint32_t CALL API_GetMemStatus(int* hDevice) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}
uint32_t CALL API_GetMemCounter(int* hDevice) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int CALL API_SetParameter(int* hDevice, uint32_t uParam, uint32_t* pParam) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int CALL API_GetParameter(int* hDevice, uint32_t uParam, uint32_t* pParam) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

signed int CALL API_SetDevice(int a1, int a2) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

signed int CALL API_SetCommand(int a1, int a2, int a3, int* a4) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

signed int CALL API_SetSysControl(int a1, int a2, int* a3) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

signed int CALL API_GetSysControl(int a1, int a2, int* a3) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int CALL API_TestReset(int a1) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

signed int API_DebugReset(int a1, ...) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int CALL API_GetBoardType(int a1) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int CALL API_GetCardDataSize(int a1) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int CALL API_GetFirmDate(int a1) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int API_SystemCommand(int a1, char a2, ...) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int CALL API_CalcCheckSum(DWORD* a1, int a2, int a3) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int CALL API_GetCheckSumResult(int a1) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int CALL API_BlockRead(int a1, int a2, int a3, SIZE_T dwBytes) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int CALL API_GetBlockReadResult(int a1, void* a2) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

int CALL API_BlockWrite(int a1, int a2, int a3, SIZE_T dwBytes, void* a5) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

signed int CALL API_GetDebugParam(int a1, int a2, DWORD* a3) {
    dprintf("Y3: %s\n", __func__);
    return 0;
}

uint32_t convert_string_to_uint(const char* firmName) {
    uint32_t result = 0;

    // Iterate over each character in the string and construct the uint32_t
    for (int i = 0; i < 4; i++) {
        result |= (uint32_t)firmName[i] << (i * 8);
    }

    return result;
}
