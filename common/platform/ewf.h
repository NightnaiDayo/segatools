#pragma once

#include <windows.h>

#include <stdbool.h>

#define EWF_MAX_VIRTUAL_FILES 255
#define EWF_DEFAULT_FILE_BUFFER_SIZE 1024

struct ewf_config {
    bool enable;
    bool full;
};

// A virtual file that we are storing in memory.
struct ewf_virtual_file {
    // The handle that we use internally. This is NULL for a handle that doesn't exist and non-NULL for a handle that maps to a virtual file.
    HANDLE virtual_handle;
    // The path that is being virtualized (ex. C:\sample.txt).
    wchar_t path[MAX_PATH];
    // The current length of the virtual file.
    size_t length;
    // The current length of the allocated data buffer.
    size_t alloc_length;
    // Pointer to some buffer holding file data. This is guaranteed to be at least length bytes in size. If length is zero, this may be NULL.
    void* data;
};

// An open handle to a ewf_virtual_file.
struct ewf_real_handle {
    // The "real" handle passed to the application as a result of CreateFile, etc.
    HANDLE real_handle;
    // The virtual file the real handle points to.
    struct ewf_virtual_file* virtual_file;
    // The current read offset.
    size_t offset;
};

HRESULT ewf_hook_init(const struct ewf_config* config);
