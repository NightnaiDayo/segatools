// A simplified version of NeighTools' UnityDoorstop, allowing mod loaders
// like BepInEx to be loaded into Unity games.
//
// SPDX-License-Identifier: CC0
// https://github.com/NeighTools/UnityDoorstop
#include <stdbool.h>

#include <pathcch.h>
#include <psapi.h>

#include "hooklib/procaddr.h"
#include "util/dprintf.h"

#include "doorstop.h"
#include "mono.h"
#include "util.h"

static void * my_mono_jit_init_version(const char *root_domain_name, const char *runtime_version);
void doorstop_invoke(void *domain);

static char module_name[MAX_PATH];
static bool doorstop_hook_initted;
static struct unity_config unity_config;
static struct hook_symbol unity_mono_syms[] = {
{
        .name = "mono_jit_init_version",
        .patch = my_mono_jit_init_version,
    }
};

void doorstop_mono_hook_init(const struct unity_config *cfg, HINSTANCE module) {
    if (doorstop_hook_initted || cfg->target_assembly[0] == 0) {
        return;
    }

    GetModuleBaseNameA(GetCurrentProcess(), module, module_name, MAX_PATH);

    memcpy(&unity_config, cfg, sizeof(*cfg));
    load_mono_functions(module);
    proc_addr_table_push(module_name, unity_mono_syms, _countof(unity_mono_syms));

    doorstop_hook_initted = true;
}

static void * my_mono_jit_init_version(const char *root_domain_name, const char *runtime_version) {
    dprintf("Unity: Starting Mono domain \"%s\"\n", root_domain_name);

    SetEnvironmentVariableW(L"DOORSTOP_DLL_SEARCH_DIRS", widen(mono_assembly_getrootdir()));

    void* domain = mono_jit_init_version(root_domain_name, runtime_version);

    doorstop_invoke(domain);
    return domain;
}

void doorstop_invoke(void* domain) {
    if (GetEnvironmentVariableW(L"DOORSTOP_INITIALIZED", NULL, 0) != 0) {
        dprintf("Unity: Doorstop is already initialized.\n");
        return;
    }

    SetEnvironmentVariableW(L"DOORSTOP_INITIALIZED", L"TRUE");

    mono_thread_set_main(mono_thread_current());

    if (mono_domain_set_config) {
#define CONFIG_EXT L".config"

        wchar_t exe_path[MAX_PATH];
        size_t exe_path_len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
        wchar_t *folder_name = wcsdup(exe_path);

        PathCchRemoveFileSpec(folder_name, exe_path_len + 1);

        char *exe_path_n = narrow(exe_path);
        char *folder_name_n = narrow(folder_name);

        dprintf("Unity: Setting config paths: base dir: %s; config path: %s\n", folder_name_n, exe_path_n);

        mono_domain_set_config(domain, folder_name_n, exe_path_n);

        free(folder_name);
        free(exe_path_n);
        free(folder_name_n);

#undef CONFIG_EXT
    }

    SetEnvironmentVariableW(L"DOORSTOP_INVOKE_DLL_PATH", unity_config.target_assembly);

    char *assembly_dir = mono_assembly_getrootdir();
    dprintf("Unity: Assembly directory: %s\n", assembly_dir);

    SetEnvironmentVariableA("DOORSTOP_MANAGED_FOLDER_DIR", assembly_dir);

    wchar_t app_path[MAX_PATH];
    GetModuleFileNameW(NULL, app_path, MAX_PATH);
    SetEnvironmentVariableW(L"DOORSTOP_PROCESS_PATH", app_path);

    char* dll_path = narrow(unity_config.target_assembly);

    dprintf("Unity: Loading assembly: %s\n", dll_path);

    void* assembly = mono_domain_assembly_open(domain, dll_path);

    if (!assembly) {
        dprintf("Unity: Failed to load assembly\n");
        free(dll_path);
        return;
    }

    void *image = mono_assembly_get_image(assembly);

    if (!image) {
        dprintf("Unity: Assembly image doesn't exist\n");
        free(dll_path);
        return;
    }

    void *desc = mono_method_desc_new("*:Main", FALSE);
    void *method = mono_method_desc_search_in_image(desc, image);

    if (!method) {
        dprintf("Unity: Assembly does not have a valid entrypoint.\n");
        free(dll_path);
        return;
    }

    void *signature = mono_method_signature(method);
    UINT32 params = mono_signature_get_param_count(signature);
    void **args = NULL;

    if (params == 1) {
        // If there is a parameter, it's most likely a string[].
        void *args_array = mono_array_new(domain, mono_get_string_class(), 0);
        args = malloc(sizeof(void*) * 1);
        args[0] = args_array;
    }

    dprintf("Unity: Invoking method %p\n", method);

    void *exc = NULL;
    mono_runtime_invoke(method, NULL, args, &exc);

    if (exc) {
        dprintf("Unity: Error invoking method!\n");

        void *ex_class = mono_get_exception_class();
        void *to_string_desc = mono_method_desc_new("*:ToString()", FALSE);
        void* to_string_method = mono_method_desc_search_in_class(to_string_desc, ex_class);

        mono_method_desc_free(to_string_desc);

        if (to_string_method) {
            void* real_to_string_method = mono_object_get_virtual_method(exc, to_string_method);
            void* exc2 = NULL;
            void* str = mono_runtime_invoke(real_to_string_method, exc, NULL, &exc2);

            if (!exc2) {
                char* exc_str = mono_string_to_utf8(str);
                dprintf("Unity: Error message: %s\n", exc_str);
            }
        }
    }

    mono_method_desc_free(desc);
    free(dll_path);

    if (args) {
        free(args);
        args = NULL;
    }
}
