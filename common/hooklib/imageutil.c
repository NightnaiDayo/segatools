#include <windows.h>
#include <stdbool.h>
#include <stdlib.h>

#include "imageutil.h"

// copy pasted from https://dev.s-ul.net/domeori/c310emu
#define BITMAPHEADERSIZE 0x36

int ConvertDataToBitmap(
    DWORD dwBitCount,
    DWORD dwWidth, DWORD dwHeight,
    PBYTE pbInput, DWORD cbInput,
    PBYTE pbOutput, DWORD cbOutput,
    PDWORD pcbResult,
    bool pFlip) {
    if (!pbInput || !pbOutput || dwBitCount < 8) return -3;

    if (cbInput < (dwWidth * dwHeight * dwBitCount / 8)) return -3;

    PBYTE pBuffer = malloc(cbInput);
    if (!pBuffer) return -2;

    BYTE dwColors = (BYTE)(dwBitCount / 8);
    if (!dwColors) {
        free(pBuffer);
        return -1;
    }

    UINT16 cbColors;
    RGBQUAD pbColors[256];

    switch (dwBitCount) {
        case 1:
            cbColors = 1;
            break;
        case 2:
            cbColors = 4;
            break;
        case 4:
            cbColors = 16;
            break;
        case 8:
            cbColors = 256;
            break;
        default:
            cbColors = 0;
            break;
    }

    if (cbColors) {
        BYTE dwStep = (BYTE)(256 / cbColors);

        for (UINT16 i = 0; i < cbColors; ++i) {
            pbColors[i].rgbRed = dwStep * i;
            pbColors[i].rgbGreen = dwStep * i;
            pbColors[i].rgbBlue = dwStep * i;
            pbColors[i].rgbReserved = 0;
        }
    }

    DWORD dwTable = cbColors * sizeof(RGBQUAD);
    DWORD dwOffset = BITMAPHEADERSIZE + dwTable;

    // calculate the padded row size, again
    DWORD dwLineSize = (dwWidth * dwBitCount / 8 + 3) & ~3;

    BITMAPFILEHEADER bFile = {0};
    BITMAPINFOHEADER bInfo = {0};

    bFile.bfType = 0x4D42;  // MAGIC
    bFile.bfSize = dwOffset + cbInput;
    bFile.bfOffBits = dwOffset;

    bInfo.biSize = sizeof(BITMAPINFOHEADER);
    bInfo.biWidth = dwWidth;
    bInfo.biHeight = dwHeight;
    bInfo.biPlanes = 1;
    bInfo.biBitCount = (WORD)dwBitCount;
    bInfo.biCompression = BI_RGB;
    bInfo.biSizeImage = cbInput;

    if (cbOutput < bFile.bfSize) {
        free(pBuffer);
        return -1;
    }

    // Flip the image (if necessary) and add padding to each row
    if (pFlip) {
        for (size_t i = 0; i < dwHeight; i++) {
            for (size_t j = 0; j < dwWidth; j++) {
                for (size_t k = 0; k < dwColors; k++) {
                    // Calculate the position in the padded buffer
                    // Make sure to also flip the colors from RGB to BRG
                    size_t x = (dwHeight - i - 1) * dwLineSize + (dwWidth - j - 1) * dwColors + (dwColors - k - 1);
                    size_t y = (dwHeight - i - 1) * dwWidth * dwColors + j * dwColors + k;
                    *(pBuffer + x) = *(pbInput + y);
                }
            }
        }
    } else {
        for (size_t i = 0; i < dwHeight; i++) {
            for (size_t j = 0; j < dwWidth; j++) {
                for (size_t k = 0; k < dwColors; k++) {
                    // Calculate the position in the padded buffer
                    size_t x = i * dwLineSize + j * dwColors + (dwColors - k - 1);
                    size_t y = (dwHeight - i - 1) * dwWidth * dwColors + j * dwColors + k;
                    *(pBuffer + x) = *(pbInput + y);
                }
            }
        }
    }

    memcpy(pbOutput, &bFile, sizeof(BITMAPFILEHEADER));
    memcpy(pbOutput + sizeof(BITMAPFILEHEADER), &bInfo, sizeof(BITMAPINFOHEADER));
    if (cbColors) memcpy(pbOutput + BITMAPHEADERSIZE, pbColors, dwTable);
    memcpy(pbOutput + dwOffset, pBuffer, cbInput);

    *pcbResult = bFile.bfSize;

    free(pBuffer);
    return 0;
}

int WriteDataToBitmapFile(
    LPCWSTR lpFilePath, DWORD dwBitCount,
    DWORD dwWidth, DWORD dwHeight,
    PBYTE pbInput, DWORD cbInput,
    PBYTE pbMetadata, DWORD cbMetadata,
    bool pFlip) {
    if (!lpFilePath || !pbInput) return -3;

    HANDLE hFile;
    DWORD dwBytesWritten;

    hFile = CreateFileW(
        lpFilePath,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE) return -1;

    // calculate the padded row size and padded image size
    DWORD dwLineSize = (dwWidth * dwBitCount / 8 + 3) & ~3;
    DWORD dwImageSize = dwLineSize * dwHeight;

    DWORD cbResult;
    DWORD cbBuffer = dwImageSize + 0x500;
    PBYTE pbBuffer = calloc(cbBuffer, 1);
    if (!pbBuffer) return -2;

    if (ConvertDataToBitmap(dwBitCount, dwWidth, dwHeight, pbInput, dwImageSize, pbBuffer, cbBuffer, &cbResult, pFlip) < 0) {
        cbResult = -1;
        goto WriteDataToBitmapFile_End;
    }

    WriteFile(hFile, pbBuffer, cbResult, &dwBytesWritten, NULL);

    if (pbMetadata)
        WriteFile(hFile, pbMetadata, cbMetadata, &dwBytesWritten, NULL);

    CloseHandle(hFile);

    cbResult = dwBytesWritten;

WriteDataToBitmapFile_End:
    free(pbBuffer);
    return cbResult;
}

int WriteArrayToFile(LPCSTR lpOutputFilePath, LPVOID lpDataTemp, DWORD nDataSize, BOOL isAppend) {
#ifdef NDEBUG

    return nDataSize;

#else

    HANDLE hFile;
    DWORD dwBytesWritten;
    DWORD dwDesiredAccess;
    DWORD dwCreationDisposition;

    if (isAppend) {
        dwDesiredAccess = FILE_APPEND_DATA;
        dwCreationDisposition = OPEN_ALWAYS;
    } else {
        dwDesiredAccess = GENERIC_WRITE;
        dwCreationDisposition = CREATE_ALWAYS;
    }

    hFile = CreateFileA(
        lpOutputFilePath,
        dwDesiredAccess,
        FILE_SHARE_READ,
        NULL,
        dwCreationDisposition,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    WriteFile(hFile, lpDataTemp, nDataSize, &dwBytesWritten, NULL);
    CloseHandle(hFile);

    return dwBytesWritten;

#endif
}