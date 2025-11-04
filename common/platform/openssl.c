#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <intrin.h>

#include "hook/table.h"

#include "hooklib/config.h"

#include "platform/openssl.h"

#include "util/dprintf.h"

/* API hooks */

static char * __cdecl hook_getenv(const char *name);

/* Link pointers */

static char * (__cdecl *next_getenv)(const char *name);

static bool openssl_hook_initted;
static struct openssl_config openssl_config;

static const struct hook_symbol openssl_hooks[] = {
    {
        .name  = "getenv",
        .patch = hook_getenv,
        .link  = (void **) &next_getenv
    },
};

static int check_intel_sha_extension() {
    int n_ids;
    int is_intel;
    char vendor[0x20] = {0};
    int cpui[4] = {0};

    /* OpenSSL 1.0.2 beta to 1.0.2k contain bugs that crash "AM Daemon" due to
      bad SHA values on processors with the SHA extensions, such as Intel 10th 
      gen CPUs or higher.

      Credits: kagaminehaku */

    __cpuid(cpui, 0);
    n_ids = cpui[0];

    *((int *) vendor) = cpui[1];
    *((int *) (vendor + 4)) = cpui[3];
    *((int *) (vendor + 8)) = cpui[2];

    /* Check that the CPU vendor is Intel */
    is_intel = (strcmp(vendor, "GenuineIntel") == 0);

    if (is_intel && n_ids >= 7) {
        __cpuidex(cpui, 7, 0);

        /* SHA extensions supported */
        return (cpui[1] & (1 << 29)) != 0;
    }

    return 0;
}

HRESULT openssl_hook_init(const struct openssl_config *cfg)
{
    assert(cfg != NULL);

    if (!cfg->enable) {
        return S_FALSE;
    }

    if (openssl_hook_initted) {
        return S_FALSE;
    }

    if (cfg->override) {
        dprintf("OpenSSL: hook enabled.\n");
    } else if (check_intel_sha_extension()) {
        dprintf("OpenSSL: Intel CPU SHA extension detected, hook enabled.\n");
    } else {
        return S_FALSE;
    }

    openssl_hook_initted = true;

    memcpy(&openssl_config, cfg, sizeof(*cfg));
    hook_table_apply(
        NULL,
        "msvcr110.dll",
        openssl_hooks,
        _countof(openssl_hooks)
    );

    return S_OK;
}

static char * __cdecl hook_getenv(const char *name)
{
    /* Overrides OpenSSL's Intel CPU identification. This disables the OpenSSL
       code check for SHA extensions. */
    if (name && strcmp(name, "OPENSSL_ia32cap") == 0) {
        static char override[] = "~0x20000000";

        dprintf("OpenSSL: Overriding OPENSSL_ia32cap -> %s\n", override);
        
        return override;
    }

    char *real_val = next_getenv(name);

    return real_val;
}
