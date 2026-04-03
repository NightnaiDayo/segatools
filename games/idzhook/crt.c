#include <windows.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "hook/table.h"

#include "crt.h"

#include "util/dprintf.h"

/* API hooks */

static int WINAPIV hook__stat64i32(
   const char *path,
   struct _stat64i32 *buffer
);

/* Link pointers */

static int (WINAPIV *next__stat64i32)(
   const char *path,
   struct _stat64i32 *buffer
);

static const char *target_executables[] = {
    "InitialD0_DX11_Nu.exe",
    "amdaemon.exe",
};

static const size_t target_executables_len = _countof(target_executables);


static const struct hook_symbol msvcr110_hooks[] = {
    {
        .name = "_stat64i32",
        .patch = hook__stat64i32,
        .link = (void**) &next__stat64i32
    }
};

void crt_hook_init(void)
{
    dprintf("CRT: Hooks init.\n");
    
    hook_table_apply(
        NULL,
        "msvcr110.dll",
        msvcr110_hooks,
        _countof(msvcr110_hooks));
}

static int __cdecl hook__stat64i32(
   const char *path,
   struct _stat64i32 *buffer
)
{
    const char *target_executable;

    if (path != NULL) {
        for (size_t i = 0; i < target_executables_len; i++) {
            target_executable = target_executables[i];

            if (strstr(path, target_executable)) {
                dprintf("CRT: Bypassing size check for %s\n", path);
                return 0;
            }
        }
    }

    return next__stat64i32(path, buffer);
}
