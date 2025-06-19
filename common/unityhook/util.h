#pragma once

#include <windows.h>

wchar_t *widen(const char *str) {
    const int reqsz = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    wchar_t *result = malloc(reqsz * sizeof(wchar_t));

    MultiByteToWideChar(CP_UTF8, 0, str, -1, result, reqsz);
    return result;
}

char *narrow(const wchar_t *str) {
    const int reqsz = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
    char *result = malloc(reqsz * sizeof(char));

    WideCharToMultiByte(CP_UTF8, 0, str, -1, result, reqsz, NULL, NULL);
    return result;
}

