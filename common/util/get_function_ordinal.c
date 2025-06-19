#include "get_function_ordinal.h"

DWORD get_function_ordinal(const char* dllName, const char* functionName) {
    HMODULE hModule = LoadLibraryA(dllName);
    if (!hModule) {
        dprintf("Failed to load DLL: %s\n", dllName);
        return 0;
    }

    ULONG size;
    PIMAGE_EXPORT_DIRECTORY exportDir = (PIMAGE_EXPORT_DIRECTORY)ImageDirectoryEntryToData(
            hModule, TRUE, IMAGE_DIRECTORY_ENTRY_EXPORT, &size);
    if (!exportDir) {
        dprintf("Failed to get export table\n");
        FreeLibrary(hModule);
        return 0;
    }

    DWORD* functionNames = (DWORD*)((BYTE*)hModule + exportDir->AddressOfNames);
    WORD* ordinals = (WORD*)((BYTE*)hModule + exportDir->AddressOfNameOrdinals);

    for (DWORD i = 0; i < exportDir->NumberOfNames; ++i) {
        char* name = (char*)((BYTE*)hModule + functionNames[i]);
        if (strcmp(name, functionName) == 0) {
            DWORD ordinal = ordinals[i] + exportDir->Base;
            FreeLibrary(hModule);
            return ordinal;
        }
    }

    dprintf("Function not found: %s\n", functionName);
    FreeLibrary(hModule);
    return 0;
}