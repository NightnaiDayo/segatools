#include <windows.h>
#include <shellapi.h>

#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <sys/time.h>

#include "hook/table.h"
#include "hooklib/dll.h"
#include "util/dprintf.h"

#include "ekthook/config.h"
#include "ekthook/y3.h"

#include <assert.h>

#define CALL __cdecl

CALL float API_DLLVersion();
CALL uint32_t API_GetLastError(int* hDevice);
CALL uint32_t API_GetErrorMessage(uint32_t errNo, char* szMessage, int numBytes);
CALL int* API_Connect(char* szPortName);
CALL int API_Close(int* hDevice);
CALL int API_Start(int* hDevice);
CALL int API_Stop(int* hDevice);
CALL float API_GetFirmVersion(int* hDevice);
CALL uint32_t API_GetFirmName(int* hDevice);
CALL uint32_t API_GetTargetCode(int* hDevice);
CALL uint32_t API_GetStatus(int* hDevice);
CALL uint32_t API_GetCounter(int* hDevice);
CALL int API_ClearError(int* hDevice);
CALL int API_Reset(int* hDevice, bool isHardReset);
CALL int API_GetCardInfo(int* hDevice, int numCards, struct CardInfo* pCardInfo);
CALL int API_GetCardInfoCharSize();
CALL int API_FirmwareUpdate(int* hDevice, uint32_t address, uint32_t size, uint8_t* buffer);
CALL int API_Calibration(int* hDevice, int calib);
CALL int API_GetCalibrationResult(int* hDevice, int calib, uint32_t* result);
CALL uint32_t API_GetProcTime(int* hDevice);
CALL uint32_t API_GetMemStatus(int* hDevice);
CALL uint32_t API_GetMemCounter(int* hDevice);
CALL int API_SetParameter(int* hDevice, uint32_t uParam, uint32_t* pParam);
CALL int API_GetParameter(int* hDevice, uint32_t uParam, uint32_t* pParam);

signed int CALL API_SetDevice(int a1, int a2);
signed int CALL API_SetCommand(int a1, int a2, int a3, int *a4);
signed int CALL API_SetSysControl(int a1, int a2, int *a3);
signed int CALL API_GetSysControl(int a1, int a2, int *a3);
int CALL API_TestReset(int a1);
signed int API_DebugReset(int a1, ...);
int CALL API_GetBoardType(int a1);
int CALL API_GetCardDataSize(int a1);
int CALL API_GetFirmDate(int a1);
int API_SystemCommand(int a1, char a2, ...);
int CALL API_CalcCheckSum(DWORD *a1, int a2, int a3);
int CALL API_GetCheckSumResult(int a1);
int CALL API_BlockRead(int a1, int a2, int a3, SIZE_T dwBytes);
int CALL API_GetBlockReadResult(int a1, void *a2);
int CALL API_BlockWrite(int a1, int a2, int a3, SIZE_T dwBytes, void *a5);
signed int CALL API_GetDebugParam(int a1, int a2, DWORD *a3);

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

static const int* Y3_COM_FIELD = (int*)10;
static const int* Y3_COM_PRINT = (int*)11;


void y3_hook_init(const struct y3_config *cfg, HINSTANCE self){
    assert(cfg != NULL);

    if (!cfg->enable) {
        return;
    }

    memcpy(&y3_config, cfg, sizeof(*cfg));

    hook_table_apply(NULL, "Y3CodeReaderNE.dll", Y3_hooks, _countof(Y3_hooks));

    if (self != NULL) {
        dll_hook_push(self, L"Y3CodeReaderNE.dll");
    }

    memset(card_data, 0, sizeof(card_data));

    dprintf("Y3: enabled\n");
}

CALL float API_DLLVersion(){
    dprintf("Y3: %s\n", __func__);
    return 1;
}
CALL uint32_t API_GetLastError(int* hDevice){
    dprintf("Y3: %s\n", __func__);
    if (!y3_config.enable) {
        return 1;
    }
    return 0;
}
CALL uint32_t API_GetErrorMessage(uint32_t errNo, char* szMessage, int numBytes){
    dprintf("Y3: %s\n", __func__);
    if (!y3_config.enable) {
        strcpy_s(szMessage, numBytes, "Y3Emu is disabled but emulator DLL is called");
    } else {
        strcpy_s(szMessage, numBytes, "No error - Y3Emu");
    }
    return 0;
}
CALL int* API_Connect(char* szPortName){
    dprintf("Y3: %s(%s)\n", __func__, szPortName);
if ((GetAsyncKeyState(0x11) & 0x8000) && (GetAsyncKeyState(0x42) & 0x8000)) {
        dprintf("Y3: Ctrl+B is active!\n");
        return NULL;
    }
    if (!y3_config.enable) {
        return NULL;
    } else {
        char number[2];
        strncpy(number, szPortName+3, 2);
        return (int*)(uintptr_t)atoi(number);
    }
}
CALL int API_Close(int* hDevice){
    dprintf("Y3: %s(%p)\n", __func__, hDevice);
    return 0;
}
CALL int API_Start(int* hDevice){
    dprintf("Y3: %s(%p)\n", __func__, hDevice);
    return 0;
}
CALL int API_Stop(int* hDevice){
    dprintf("Y3: %s(%p)\n", __func__, hDevice);
    return 0;
}
CALL float API_GetFirmVersion(int* hDevice){
    dprintf("Y3: %s(%p)\n", __func__, hDevice);
    return 1;
}
CALL uint32_t API_GetFirmName(int* hDevice){
    dprintf("Y3: %s(%p)\n", __func__, hDevice);
    if (hDevice == Y3_COM_FIELD){
        dprintf("This Y3 device is FIELD\n");
        return 1380992595;
    } else if (hDevice == Y3_COM_PRINT){
        dprintf("This Y3 device is PRINTER\n");
        //DebugBreak();
        return 1414680659;
    } else {
        dprintf("This Y3 device is UNKNOWN\n");
    }
    return 0;
}
CALL uint32_t API_GetTargetCode(int* hDevice){
    dprintf("Y3: %s(%p)\n", __func__, hDevice);
    if (hDevice == Y3_COM_FIELD){
        dprintf("This Y3 device is FIELD\n");
        return 810698323;
    } else if (hDevice == Y3_COM_PRINT){
        dprintf("This Y3 device is PRINTER\n");
        //DebugBreak();
        return 810831955;
    } else {
        dprintf("This Y3 device is UNKNOWN\n");
    }
    return 1162760014;
}
CALL uint32_t API_GetStatus(int* hDevice){
    //dprintf("Y3: %s\n", __func__);
    return 0;
}
CALL uint32_t API_GetCounter(int* hDevice){
    //dprintf("Y3: %s\n", __func__);
    return 0;
}
CALL int API_ClearError(int* hDevice){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
CALL int API_Reset(int* hDevice, bool isHardReset){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
CALL int API_GetCardInfo(int* hDevice, int numCards, struct CardInfo* pCardInfo){
    //dprintf("Y3: %s(%p), %d\n", __func__, hDevice, numCards);
    // ret = num cards
    // numCards = max cards

    if (hDevice == Y3_COM_PRINT){
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
        pCardInfo[0].ubChar3.Data = 0x0; // 40
        pCardInfo[0].ubChar4.Data = 0;
        pCardInfo[0].ubChar5.Data = 0;
        return 1;
    } else if (numCards == MAX_CARD_SIZE){
        memcpy(pCardInfo, card_data, sizeof(card_data));
        return MAX_CARD_SIZE;
    }

    return 0;
}
CALL int API_GetCardInfoCharSize(){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
CALL int API_FirmwareUpdate(int* hDevice, uint32_t address, uint32_t size, uint8_t* buffer){
    dprintf("Y3: %s\n", __func__);
    return 1; // not supported
}
CALL int API_Calibration(int* hDevice, int calib){
    dprintf("Y3: %s\n", __func__);
    return 1;
}
CALL int API_GetCalibrationResult(int* hDevice, int calib, uint32_t* result){
    dprintf("Y3: %s\n", __func__);
    return 1;
}
CALL uint32_t API_GetProcTime(int* hDevice){
    //dprintf("Y3: %s\n", __func__);
    return 0;
}
CALL uint32_t API_GetMemStatus(int* hDevice){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
CALL uint32_t API_GetMemCounter(int* hDevice){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
CALL int API_SetParameter(int* hDevice, uint32_t uParam, uint32_t* pParam){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
CALL int API_GetParameter(int* hDevice, uint32_t uParam, uint32_t* pParam){
    dprintf("Y3: %s\n", __func__);
    return 0;
}

signed int CALL API_SetDevice(int a1, int a2){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
signed int CALL API_SetCommand(int a1, int a2, int a3, int *a4){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
signed int CALL API_SetSysControl(int a1, int a2, int *a3){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
signed int CALL API_GetSysControl(int a1, int a2, int *a3){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
int CALL API_TestReset(int a1){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
signed int API_DebugReset(int a1, ...){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
int CALL API_GetBoardType(int a1){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
int CALL API_GetCardDataSize(int a1){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
int CALL API_GetFirmDate(int a1){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
int API_SystemCommand(int a1, char a2, ...){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
int CALL API_CalcCheckSum(DWORD *a1, int a2, int a3){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
int CALL API_GetCheckSumResult(int a1){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
int CALL API_BlockRead(int a1, int a2, int a3, SIZE_T dwBytes){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
int CALL API_GetBlockReadResult(int a1, void *a2){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
int CALL API_BlockWrite(int a1, int a2, int a3, SIZE_T dwBytes, void *a5){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
signed int CALL API_GetDebugParam(int a1, int a2, DWORD *a3){
    dprintf("Y3: %s\n", __func__);
    return 0;
}
