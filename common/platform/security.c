#include <windows.h>
#include <bcrypt.h>

#define STATUS_SUCCESS ((NTSTATUS)0x00000000)

#include <stddef.h>
#include <stdlib.h>

#include "hook/table.h"

#include "platform/security.h"

#include "util/dprintf.h"
#include "util/dump.h"

static NTSTATUS __stdcall my_BCryptDecrypt(
    BCRYPT_KEY_HANDLE hKey,
    PUCHAR pbInput,
    ULONG cbInput,
    void* pPaddingInfo,
    PUCHAR pbIV,
    ULONG cbIV,
    PUCHAR pbOutput,
    ULONG cbOutput,
    ULONG* pcbResult,
    ULONG dwFlags);

static NTSTATUS (__stdcall *next_BCryptDecrypt)(
    BCRYPT_KEY_HANDLE hKey,
    PUCHAR pbInput,
    ULONG cbInput,
    void* pPaddingInfo,
    PUCHAR pbIV,
    ULONG cbIV,
    PUCHAR pbOutput,
    ULONG cbOutput,
    ULONG* pcbResult,
    ULONG dwFlags);

static NTSTATUS __stdcall my_BCryptEncrypt(
    BCRYPT_KEY_HANDLE hKey,
    PUCHAR pbInput,
    ULONG cbInput,
    void* pPaddingInfo,
    PUCHAR pbIV,
    ULONG cbIV,
    PUCHAR pbOutput,
    ULONG cbOutput,
    ULONG* pcbResult,
    ULONG dwFlags);

static NTSTATUS (__stdcall *next_BCryptEncrypt)(
    BCRYPT_KEY_HANDLE hKey,
    PUCHAR pbInput,
    ULONG cbInput,
    void* pPaddingInfo,
    PUCHAR pbIV,
    ULONG cbIV,
    PUCHAR pbOutput,
    ULONG cbOutput,
    ULONG* pcbResult,
    ULONG dwFlags);

static const struct hook_symbol security_hook_syms[] = {
    {
        .name = "BCryptDecrypt",
        .patch = my_BCryptDecrypt,
        .link = (void **) &next_BCryptDecrypt,
    },
    {
        .name = "BCryptEncrypt",
        .patch = my_BCryptEncrypt,
        .link = (void **) &next_BCryptEncrypt,
    }
};

void LogKeyProperties(BCRYPT_KEY_HANDLE hKey) {
    NTSTATUS status;
    ULONG cbData;

    // Retrieve the algorithm name
    WCHAR algorithmName[256];
    ULONG cbAlgorithmName = sizeof(algorithmName);

    status = BCryptGetProperty(hKey, BCRYPT_ALGORITHM_NAME, (PUCHAR) algorithmName, cbAlgorithmName * sizeof(WCHAR),
                               &cbData, 0);
    if (status == STATUS_SUCCESS) {
        dprintf("Algorithm: %ls\n", algorithmName);
    } else {
        dprintf("Algorithm: failed to retrieve algorithm name (0x%08lX)\n", status);
    }

    // Export the key value
    DWORD keyBlobSize = 0;
    status = BCryptExportKey(hKey, NULL, BCRYPT_KEY_DATA_BLOB, NULL, 0, &keyBlobSize, 0);
    if (status != STATUS_SUCCESS) {
        dprintf("KEY: failed to determine key blob size (0x%08lX)\n", status);
        return;
    }

    PUCHAR keyBlob = (PUCHAR) malloc(keyBlobSize);
    if (!keyBlob) {
        dprintf("KEY: failed the memory allocation for key blob.\n");
        return;
    }

    status = BCryptExportKey(hKey, NULL, BCRYPT_KEY_DATA_BLOB, keyBlob, keyBlobSize, &keyBlobSize, 0);
    if (status == STATUS_SUCCESS) {
        int headerSize = sizeof(BCRYPT_KEY_DATA_BLOB_HEADER);
        dprintf("KEY (%lu bytes):\n", keyBlobSize - headerSize);
        dump(keyBlob + headerSize, keyBlobSize - headerSize);
    } else {
        dprintf("KEY: failed to export key value (0x%08lX)\n", status);
    }

    free(keyBlob);
}


static NTSTATUS __stdcall my_BCryptDecrypt(
    BCRYPT_KEY_HANDLE hKey,
    PUCHAR pbInput,
    ULONG cbInput,
    void* pPaddingInfo,
    PUCHAR pbIV,
    ULONG cbIV,
    PUCHAR pbOutput,
    ULONG cbOutput,
    ULONG* pcbResult,
    ULONG dwFlags) {
    dprintf("BCrypt: Decrypt\n");
    dprintf("FLAGS: %lx\n", dwFlags);
    LogKeyProperties(hKey);
    dprintf("IV:\n");
    dump(pbIV, cbIV);
    dprintf("ENCRYPTED:\n");
    dump(pbInput, cbInput);
    NTSTATUS ret = next_BCryptDecrypt(hKey, pbInput, cbInput, pPaddingInfo, pbIV, cbIV, pbOutput, cbOutput, pcbResult,
                                      dwFlags);
    dprintf("Operation Result: %lx\n", ret);
    if (ret == 0) {
        dprintf("Decrypted (%ld, max buf: %ld):\n", *pcbResult, cbOutput);
        if (pbOutput != NULL) {
            dump(pbOutput, *pcbResult);
        } else {
            dprintf("pbOutput == NULL!\n");
        }
    }

    return ret;
}


static NTSTATUS __stdcall my_BCryptEncrypt(
    BCRYPT_KEY_HANDLE hKey,
    PUCHAR pbInput,
    ULONG cbInput,
    void* pPaddingInfo,
    PUCHAR pbIV,
    ULONG cbIV,
    PUCHAR pbOutput,
    ULONG cbOutput,
    ULONG* pcbResult,
    ULONG dwFlags) {
    dprintf("BCrypt: Encrypt\n");
    dprintf("FLAGS: %lx\n", dwFlags);
    LogKeyProperties(hKey);
    dprintf("IV:\n");
    dump(pbIV, cbIV);
    dprintf("Input data (%ld):\n", cbInput);
    dump(pbOutput, *pcbResult);
    NTSTATUS ret = next_BCryptEncrypt(hKey, pbInput, cbInput, pPaddingInfo, pbIV, cbIV, pbOutput, cbOutput, pcbResult,
                                      dwFlags);
    dprintf("Operation Result: %lx\n", ret);
    if (ret == 0) {
        dprintf("First 16 bytes of encrypted data (%ld, max buf: %ld):\n", *pcbResult, cbOutput);
        if (pbOutput != NULL) {
            dump(pbOutput, min(16, *pcbResult));
        } else {
            dprintf("pbOutput == NULL!\n");
        }
    }
    return ret;
}

HRESULT security_hook_init() {
    security_hook_insert_hooks(NULL);

    return S_OK;
}

void security_hook_insert_hooks(HMODULE mod) {

    char value[8];
    if (!GetEnvironmentVariableA("BCRYPT_DUMP_ENABLED", value, 8)) {
        return;
    }

    if (value[0] != '1') {
        return;
    }

    dprintf("BCrypt: dumping enabled\n");

    hook_table_apply(
        mod,
        "BCrypt.dll",
        security_hook_syms,
        _countof(security_hook_syms));
}
