#include <windows.h>
#include <shlwapi.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "hook/iohook.h"
#include "hook/procaddr.h"

#include "platform/ewf.h"

#include <time.h>

#include "hooklib/path.h"
#include "util/dprintf.h"
#include "util/dump.h"

/* EWF hook */

const static struct ewf_config* ewf_config;
static struct ewf_virtual_file virtual_file_table[EWF_MAX_VIRTUAL_FILES] = {0};
static struct ewf_real_handle handle_table[EWF_MAX_HANDLES] = {0};
static CRITICAL_SECTION file_table_lock;
static CRITICAL_SECTION handle_table_lock;
static wchar_t windows_directory[MAX_PATH];

static const wchar_t* default_drive = L"C:\\";
static const wchar_t default_paths[][MAX_PATH] = {
    L"alib.conf",
    L"cacert.pem",
    L"first_ar.conf",
    L"last_pras.log",
    L"last_shime.log",
    L"play_history.csv"
};

/* Helper functions */

static HRESULT atow(const char* string, wchar_t** result) {
    if (string == NULL) {
        *result = NULL;
        return S_FALSE;
    }
    const size_t n = strlen(string);
    wchar_t* widestring = malloc((n + 1) * sizeof(wchar_t));
    if (widestring == NULL) {
        return E_OUTOFMEMORY;
    }
    mbstowcs_s(NULL, widestring, n + 1, string, n);

    *result = widestring;
    return S_OK;
}

static inline bool wprefix(const wchar_t* pre, const wchar_t* str) {
    return wcsncmp(pre, str, wcslen(pre)) == 0;
}

static inline bool wsuffix(const wchar_t* suffix, const wchar_t* str) {
    if (str == NULL || suffix == NULL) {
        return false;
    }
    const size_t str_len = wcslen(str);
    const size_t suf_len = wcslen(suffix);
    if (suf_len > str_len) {
        return false;
    }
    return wcsncmp(str + str_len - suf_len, suffix, suf_len) == 0;
}


static BOOL ewf_needs_virtualization(const wchar_t* path) {
    if (path == NULL) {
        return FALSE;
    }
    if (ewf_config->full) {
        if (wprefix(default_drive, path) && !wprefix(windows_directory, path)) {
            return TRUE;
        }
    } else {
        for (int i = 0; i < _countof(default_paths); i++) {
            if (wsuffix(default_paths[i], path)) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

static struct ewf_virtual_file* ewf_get_virtual_file(HANDLE handle) {
    struct ewf_virtual_file* ret = NULL;
    EnterCriticalSection(&file_table_lock);
    for (int i = 0; i < EWF_MAX_VIRTUAL_FILES; i++) {
        if (virtual_file_table[i].virtual_handle == handle) {
            ret = &virtual_file_table[i];
            break;
        }
    }
    LeaveCriticalSection(&file_table_lock);
    return ret;
}

static struct ewf_virtual_file* ewf_find_virtual_file(const wchar_t* path) {
    if (path == NULL) {
        return NULL;
    }
    HANDLE ret = NULL;
    EnterCriticalSection(&file_table_lock);
    for (int i = 0; i < EWF_MAX_VIRTUAL_FILES; i++) {
        if (virtual_file_table[i].virtual_handle != NULL && wcscmp(virtual_file_table[i].path, path) == 0) {
            ret = &virtual_file_table[i];
            break;
        }
    }
    LeaveCriticalSection(&file_table_lock);
    return ret;
}

static struct ewf_virtual_file* ewf_create_virtual_file(const wchar_t* path) {
    assert(path != NULL);
    struct ewf_virtual_file* ret = NULL;
    EnterCriticalSection(&file_table_lock);
    for (int i = 0; i < EWF_MAX_VIRTUAL_FILES; i++) {
        if (virtual_file_table[i].virtual_handle == NULL) {
            struct ewf_virtual_file* h = &virtual_file_table[i];
            HRESULT hr = iohook_open_nul_fd(&h->virtual_handle);
            if (!SUCCEEDED(hr)) {
                dprintf("EWF: Could not create handle: Failed to get NUL handle: %lx", hr);
                break;
            }
            dprintf("EWF: Created virtual file: %ls\n", path);
            wcscpy_s(h->path, MAX_PATH, path);
            h->length = 0;
            h->data = NULL;
            ret = h;
            break;
        }
    }
    if (ret == NULL) {
        dprintf("EWF: Could not create handle: Too many virtualized files\n");
    }
    LeaveCriticalSection(&file_table_lock);
    return ret;
}

static BOOL ewf_delete_virtual_file(HANDLE virtual_handle) {
    if (virtual_handle == NULL) {
        return TRUE;
    }
    struct ewf_virtual_file* match = NULL;
    EnterCriticalSection(&file_table_lock);
    for (int i = 0; i < EWF_MAX_VIRTUAL_FILES; i++) {
        if (virtual_file_table[i].virtual_handle == virtual_handle) {
            match = &virtual_file_table[i];
            break;
        }
    }
    LeaveCriticalSection(&file_table_lock);
    if (match) {
        match->virtual_handle = NULL;
#if LOG_EWF
        dprintf("EWF: Deleted file: %ls\n", match->path);
#endif
        return CloseHandle(virtual_handle);
    } else {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
}

static struct ewf_real_handle* ewf_open_virtual_file(HANDLE virtual_handle) {
    struct ewf_virtual_file* file = ewf_get_virtual_file(virtual_handle);
    struct ewf_real_handle* h = NULL;
    if (file == NULL) {
        dprintf("EWF: open failed: invalid handle\n");
        return NULL;
    }
    EnterCriticalSection(&handle_table_lock);
    for (int i = 0; i < EWF_MAX_HANDLES; i++) {
        if (handle_table[i].real_handle == NULL) {
            h = &handle_table[i];
            HRESULT hr = iohook_open_nul_fd(&h->real_handle);
            if (!SUCCEEDED(hr)) {
                dprintf("EWF: Could not create handle: Failed to get NUL handle: %lx", hr);
                break;
            }
            h->virtual_file = file;
            h->offset = 0;
#if LOG_EWF
            dprintf("EWF: Virtual file opened: %ls\n", file->path);
#endif
            break;
        }
    }
    LeaveCriticalSection(&handle_table_lock);
    if (h == NULL) {
        dprintf("EWF: Could not create handle: Too many open files\n");
    }
    return h;
}

static struct ewf_real_handle* ewf_get_real_handle(HANDLE real_handle) {
    if (real_handle == NULL) {
        return NULL;
    }
    struct ewf_real_handle* match = NULL;
    EnterCriticalSection(&handle_table_lock);
    for (int i = 0; i < EWF_MAX_HANDLES; i++) {
        if (handle_table[i].real_handle == real_handle) {
            match = &handle_table[i];
            break;
        }
    }
    LeaveCriticalSection(&handle_table_lock);
    return match;
}

static BOOL ewf_close_virtual_file(HANDLE real_handle) {
    const struct ewf_virtual_file* match = NULL;
    EnterCriticalSection(&handle_table_lock);
    for (int i = 0; i < EWF_MAX_HANDLES; i++) {
        if (handle_table[i].real_handle == real_handle) {
            match = handle_table[i].virtual_file;
            handle_table[i].real_handle = NULL;
            break;
        }
    }
    LeaveCriticalSection(&handle_table_lock);
    if (match != NULL) {
#if LOG_EWF
        dprintf("EWF: Virtual file closed: %ls\n", match->path);
#endif
        return CloseHandle(real_handle);
    } else {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
}

/* Hooks */

static int __cdecl hook_stat64i32(
    const char* path,
    struct _stat64i32* buffer
);

static int (__cdecl *next_stat64i32)(
    const char* path,
    struct _stat64i32* buffer);

static const struct hook_symbol ewf_hook_syms[] = {
    {
        .name = "__imp__stat64i32",
        .patch = hook_stat64i32,
        .link = (void **) &next_stat64i32,
    },
    {
        .name = "_stat64i32",
        .patch = hook_stat64i32,
        .link = (void **) &next_stat64i32,
    }
};

/* EWF hook main functions */

static HRESULT ewf_handle_irp(struct irp* irp);

static HRESULT ewf_handle_open(struct irp* irp);

static HRESULT ewf_handle_close(const struct irp* irp, const struct ewf_real_handle* file);

static HRESULT ewf_handle_read(struct irp* irp, struct ewf_real_handle* handle);

static HRESULT ewf_handle_write(const struct irp* irp, const struct ewf_real_handle* file);

void ewf_hook_insert_hooks(HMODULE target) {
    hook_table_apply(
        target,
        "msvcr110d.dll",
        ewf_hook_syms,
        _countof(ewf_hook_syms));
    hook_table_apply(
        target,
        "msvcr110.dll",
        ewf_hook_syms,
        _countof(ewf_hook_syms));
}

HRESULT ewf_hook_init(const struct ewf_config* config) {
    assert(config != NULL);

    ewf_config = config;

    if (!config->enable) {
        return S_FALSE;
    }

    GetWindowsDirectoryW(windows_directory, MAX_PATH);

    if (config->full) {
        wchar_t executable_path[MAX_PATH];
        GetModuleFileNameW(NULL, executable_path, MAX_PATH);
        if (wprefix(default_drive, executable_path)) {
            dprintf("FATAL: EWF full virtualization cannot be enabled with the game executable being located on %ls\n",
                    default_drive);
            return E_FAIL;
        }
    }

    dprintf("EWF: Virtualizing disk I/O to %s\n",
            config->full ? "the entirety of the C:\\ drive" : "the ALPB billing directory");

    InitializeCriticalSection(&handle_table_lock);
    InitializeCriticalSection(&file_table_lock);

    ewf_hook_insert_hooks(NULL);

    return iohook_push_handler(ewf_handle_irp);
}

static HRESULT ewf_handle_irp(struct irp* irp) {
    assert(irp != NULL);

    struct ewf_real_handle* h = NULL;

    if (irp->op == IRP_OP_OPEN) {
        if (!ewf_needs_virtualization(irp->open_filename)) {
            return iohook_invoke_next(irp);
        }
    } else {
        h = ewf_get_real_handle(irp->fd);
        if (h == NULL) {
            return iohook_invoke_next(irp);
        }
    }


    switch (irp->op) {
        case IRP_OP_OPEN: return ewf_handle_open(irp);
        case IRP_OP_WRITE: return ewf_handle_write(irp, h);
        case IRP_OP_READ: return ewf_handle_read(irp, h);
        case IRP_OP_CLOSE: return ewf_handle_close(irp, h);
        default: return HRESULT_FROM_WIN32(ERROR_INVALID_FUNCTION);
    }
}

static HRESULT ewf_handle_open(struct irp* irp) {
    assert(irp != NULL);

    if (irp->ovl != NULL) {
        dprintf("EWF: async file operations not supported\n");
        return E_NOTIMPL;
    }

    struct ewf_virtual_file* f = ewf_find_virtual_file(irp->open_filename);

    if (irp->open_creation == CREATE_NEW) {
        if (f != NULL) {
            return HRESULT_FROM_WIN32(ERROR_FILE_EXISTS);
        } else {
            f = ewf_create_virtual_file(irp->open_filename);
        }
    } else if (irp->open_creation == CREATE_ALWAYS) {
        if (f != NULL) {
            SetLastError(ERROR_ALREADY_EXISTS);
#if LOG_EWF
            dprintf("EWF: File was truncated\n");
#endif
            f->length = 0;
            free(f->data);
        } else {
            f = ewf_create_virtual_file(irp->open_filename);
        }
    } else if (irp->open_creation == OPEN_EXISTING) {
        if (f == NULL) {
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }
    } else if (irp->open_creation == OPEN_ALWAYS) {
        if (f != NULL) {
            SetLastError(ERROR_ALREADY_EXISTS);
        } else {
            f = ewf_create_virtual_file(irp->open_filename);
        }
    } else if (irp->open_creation == TRUNCATE_EXISTING) {
        if (f != NULL) {
#if LOG_EWF
            dprintf("EWF: File was truncated\n");
#endif
            f->length = 0;
            free(f->data);
        } else {
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }
    } else {
        return E_INVALIDARG;
    }

    const struct ewf_real_handle* h = ewf_open_virtual_file(f->virtual_handle);
    irp->fd = h->real_handle;
    return S_OK;
}

static HRESULT ewf_handle_close(const struct irp* irp, const struct ewf_real_handle* file) {
    assert(irp != NULL);
    assert(file != NULL);

    return ewf_close_virtual_file(file->real_handle) ? S_OK : E_FAIL;
}

static HRESULT ewf_handle_read(struct irp* irp, struct ewf_real_handle* handle) {
    assert(irp != NULL);
    assert(handle != NULL);

    const struct ewf_virtual_file* f = handle->virtual_file;
    size_t pos = handle->offset;
    size_t to_read = irp->read.nbytes;
    size_t max = f->length;

    if (pos > max || pos < 0) {
        dprintf("EWF: Out-of-bounds read from %ls\n", f->path);
        return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
    }
    if (max == 0 || pos >= max || to_read == 0) {
#if LOG_EWF
        dprintf("EWF: Zero read of %d at %d/%d from %ls\n", (int) to_read, (int) pos, (int) max, f->path);
#endif
        irp->read.pos = 0;
        return S_FALSE;
    }

    if (pos + to_read > max) {
        to_read = max - pos;
    }

    memcpy(irp->read.bytes, f->data, to_read);
    irp->read.pos = to_read;
    handle->offset += to_read;

#if LOG_EWF
    dprintf("EWF: Read %d/%d bytes (offset %d, max %d) from %ls\n", (int) to_read, (int) irp->read.nbytes, (int) pos,
            (int) f->length, f->path);
    dump_iobuf(&irp->read);
#endif

    return S_OK;
}

static HRESULT ewf_handle_write(const struct irp* irp, const struct ewf_real_handle* file) {
    assert(irp != NULL);
    assert(file != NULL);

    struct ewf_virtual_file* f = file->virtual_file;
    const size_t n = irp->write.nbytes;
    const void* data = irp->write.bytes;

    if (f->length == 0) {
        const size_t initial_buf = max(n, EWF_DEFAULT_FILE_BUFFER_SIZE);
        f->data = malloc(initial_buf);
        if (f->data == NULL) {
            return E_OUTOFMEMORY;
        }
        f->alloc_length = initial_buf;
    } else if (f->length + n > f->alloc_length) {
        void* ptr = realloc(f->data, f->alloc_length + n);
        if (ptr == NULL) {
            return E_OUTOFMEMORY;
        }
        f->data = ptr;
        f->alloc_length = f->alloc_length + n;
    }

    if (memcpy_s((char *) f->data + f->length, f->alloc_length - f->length, data, n) != 0) {
        dprintf("EWF: Failed to copy %d bytes at offset %d (max %d)\n", (int) n, (int) f->length,
                (int) f->alloc_length);
        return E_NOT_SUFFICIENT_BUFFER;
    }
    f->length += n;

#if LOG_EWF
    dprintf("EWF: Write %d bytes to %ls\n", (int) n, f->path);
    dump_const_iobuf(&irp->write);
    dprintf("EWF: File content (%d, allocated %d)\n", (int) f->length, (int) f->alloc_length);
    dump(f->data, f->length);
#endif

    return S_OK;
}

// MSVC implementation detail. amdaemon depends on this succeeding, or you will be spammed with "cannot get accounting info"
static int __cdecl hook_stat64i32(
    const char* path,
    struct _stat64i32* buffer
) {
    if (buffer == NULL || path == NULL) {
        _set_errno(EINVAL);
        return -1;
    }

    wchar_t* wpath = NULL;
    wchar_t* trans;
    BOOL ok;

    HRESULT result = atow(path, &wpath);
    if (!SUCCEEDED(result)) {
        _set_errno(ENOMEM);
        return -1;
    }

    ok = path_transform_w(&trans, wpath);

    if (!ok) {
        _set_errno(EINVAL);
        return -1;
    }

    wchar_t* target_path = trans ? trans : wpath;

    if (!ewf_needs_virtualization(target_path)) {
        free(trans);
        free(wpath);
#if LOG_EWF
        dprintf("EWF: stat64i32: ignore: %s\n", path);
#endif
        return next_stat64i32(path, buffer);
    }

    const struct ewf_virtual_file* f = ewf_find_virtual_file(target_path);
    if (f == NULL) {
        free(trans);
        free(wpath);
        dprintf("EWF: stat64i32: File not found: %s\n", path);
        _set_errno(ENOENT);
        return -1;
    }

    buffer->st_gid = 0;
    buffer->st_atime = time(NULL);
    buffer->st_ctime = time(NULL);
    buffer->st_dev = 0;
    buffer->st_ino = 0;
    buffer->st_mode = _S_IFREG;
    buffer->st_mtime = time(NULL);
    buffer->st_nlink = 1;
    buffer->st_rdev = 0;
    buffer->st_size = (_off_t) f->length;
    buffer->st_uid = 0;

#if LOG_EWF
    dprintf("EWF: stat64i32: file length of %s: %d\n", path, (int) f->length);
#endif
    free(trans);
    free(wpath);

    return 0;
}
