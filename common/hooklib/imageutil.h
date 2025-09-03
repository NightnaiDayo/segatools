#pragma once

#include <windows.h>
#include <stdbool.h>

int ConvertDataToBitmap(
    DWORD dwBitCount,
    DWORD dwWidth, DWORD dwHeight,
    PBYTE pbInput, DWORD cbInput,
    PBYTE pbOutput, DWORD cbOutput,
    PDWORD pcbResult,
    bool pFlip);

int WriteDataToBitmapFile(
    LPCWSTR lpFilePath, DWORD dwBitCount,
    DWORD dwWidth, DWORD dwHeight,
    PBYTE pbInput, DWORD cbInput,
    PBYTE pbMetadata, DWORD cbMetadata,
    bool pFlip);

int WriteArrayToFile(LPCSTR lpOutputFilePath, LPVOID lpDataTemp, DWORD nDataSize, BOOL isAppend);