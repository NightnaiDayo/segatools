// A simplified version of NeighTools' UnityDoorstop, allowing mod loaders
// like BepInEx to be loaded into Unity games.
//
// SPDX-License-Identifier: CC0
// https://github.com/NeighTools/UnityDoorstop
#include <stdbool.h>
#include <stdlib.h>
#include <windows.h>
#include <wchar.h>

#include <pathcch.h>
#include <psapi.h>

#include "hook/procaddr.h"
#include "util/dprintf.h"

#include "doorstop.h"
#include "mono.h"
#include "util.h"

#define MONO_DEBUG_ARG_START "--debugger-agent=transport=dt_socket,server=y,address="
#define MONO_DEBUG_NO_SUSPEND ",suspend=n"
#define MONO_DEBUG_NO_SUSPEND_NET35 ",suspend=n,defer=y"

static void* my_mono_jit_init_version(const char* root_domain_name,
                                      const char* runtime_version);
static void my_mono_jit_parse_options(int argc, const char** argv);
static void* my_mono_debug_init(MonoDebugFormat format);

void doorstop_invoke(void* domain);

static char module_name[MAX_PATH];
static bool doorstop_hook_initted;
static bool mono_debug_init_called;
static bool mono_is_net35;
static struct unity_config unity_config;
static struct hook_symbol unity_mono_syms[] = {
    {
        .name = "mono_jit_init_version",
        .patch = my_mono_jit_init_version,
    },
    {
        .name = "mono_debug_init",
        .patch = my_mono_debug_init,
    },
    {
        .name = "mono_jit_parse_options",
        .patch = my_mono_jit_parse_options,
    }};

void doorstop_mono_hook_init(const struct unity_config* cfg, HINSTANCE module) {
    if (doorstop_hook_initted) {
        return;
    }

    GetModuleBaseNameA(GetCurrentProcess(), module, module_name, MAX_PATH);

    memcpy(&unity_config, cfg, sizeof(*cfg));
    load_mono_functions(module);
    proc_addr_table_push(NULL, module_name, unity_mono_syms,
                         _countof(unity_mono_syms));

    doorstop_hook_initted = true;
}

static void * my_mono_jit_init_version(const char *root_domain_name, const char *runtime_version) {
    dprintf("Unity: Starting Mono domain \"%s\", runtime version %s\n", root_domain_name, runtime_version);
    if (strlen(runtime_version) > 2 && (runtime_version[1] == L'2' || runtime_version[1] == L'1')) {
        mono_is_net35 = TRUE;
    }

    SetEnvironmentVariableW(L"DOORSTOP_DLL_SEARCH_DIRS", widen(mono_assembly_getrootdir()));
    my_mono_jit_parse_options(0, NULL);

    bool debugger_already_enabled = mono_debug_init_called;
    if (mono_debug_enabled) {
        debugger_already_enabled |= mono_debug_enabled();
    }

    if (!debugger_already_enabled) {
        dprintf("Unity: Detected mono debugger is not initialized; initialized it\n");
        mono_debug_init(MONO_DEBUG_FORMAT_MONO);
    }
    void* domain = mono_jit_init_version(root_domain_name, runtime_version);

    doorstop_invoke(domain);
    return domain;
}

static void my_mono_jit_parse_options(int argc, const char** argv) {
    char* debug_options = getenv(TEXT("DNSPY_UNITY_DBG2"));
    if (debug_options) {
        unity_config.debug_enable = TRUE;
    }

    if (unity_config.debug_enable) {
        dprintf("Unity: Configuring mono debug server\n");

        int size = argc + 1;
        const char** new_argv = calloc(size, sizeof(char*));
        memcpy(new_argv, argv, argc * sizeof(char*));

        //check if debug options are already set by env variable, if not, build it from config
        if (!debug_options) {
            size_t debug_args_len = strlen(MONO_DEBUG_ARG_START) +
                                    wcslen(unity_config.debug_address);
            if (!unity_config.debug_suspend) {
                if (mono_is_net35) {
                    debug_args_len += strlen(MONO_DEBUG_NO_SUSPEND_NET35);
                } else {
                    debug_args_len += strlen(MONO_DEBUG_NO_SUSPEND);
                }
            }

            debug_options = calloc(debug_args_len + 1, sizeof(wchar_t));
            strcat(debug_options, MONO_DEBUG_ARG_START);

            char* temp_address = narrow(unity_config.debug_address);
            strcat(debug_options, temp_address);
            free(temp_address);

            if (!unity_config.debug_suspend) {
                if (mono_is_net35) {
                    strcat(debug_options, MONO_DEBUG_NO_SUSPEND_NET35);
                } else {
                    strcat(debug_options, MONO_DEBUG_NO_SUSPEND);
                }
            }
            dprintf("Unity: Debug options from config: %s\n", debug_options);
        } else
            dprintf("Unity: Debug options from env DNSPY_UNITY_DBG2: %s\n",
                    debug_options);

        new_argv[argc] = debug_options;
        /*
        dprintf("Unity: mono_jit_parse_options argc: %d\n", size);
        dprintf("Unity: mono_jit_parse_options argv:\n");
        for (int i = 0; i < size; i++) {
            dprintf("  %d: %s\n", i, new_argv[i]);
        }
        dprintf("\n");
        */
        mono_jit_parse_options(size, new_argv);
        free(debug_options);
        free(new_argv);
    } else {
        mono_jit_parse_options(argc, argv);
    }
}

static void* my_mono_debug_init(MonoDebugFormat format) {
    dprintf("Unity: Start mono debug init with format %d\n",format);
    mono_debug_init_called = true;
    void* domain = mono_debug_init(format);
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

        wchar_t config_path[MAX_PATH];
        size_t config_path_len = GetModuleFileNameW(NULL, config_path, MAX_PATH);
        wchar_t *folder_name = wcsdup(config_path);

        PathCchRemoveFileSpec(folder_name, config_path_len + 1);
        wmemcpy(config_path + config_path_len, CONFIG_EXT, sizeof(CONFIG_EXT) / sizeof(CONFIG_EXT[0]));

        char *config_path_n = narrow(config_path);
        char *folder_name_n = narrow(folder_name);

        dprintf("Unity: Setting config paths: base dir: %s; config path: %s\n", folder_name_n, config_path_n);

        mono_domain_set_config(domain, folder_name_n, config_path_n);

        free(folder_name);
        free(config_path_n);
        free(folder_name_n);

#undef CONFIG_EXT
    }

    if(unity_config.target_assembly[0] == 0) {
        dprintf("Unity: No target assembly file specified, skipping doorstop invocation.\n");
        return;
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
        dprintf("Unity: Failed to load target assembly\n");
        free(dll_path);
        return;
    }

    void *image = mono_assembly_get_image(assembly);

    if (!image) {
        dprintf("Unity: Target assembly image doesn't exist\n");
        free(dll_path);
        return;
    }

    // BepInEx 5.4.23 has upgrade its doorstop version,
    // which forces entrypoint to Doorstop.Entrypoint:Start

    void *desc = mono_method_desc_new("Doorstop.Entrypoint:Start", TRUE);
    void *method = mono_method_desc_search_in_image(desc, image);

    if (!method) {
        // Fallback to old entrypoint definition.

        desc = mono_method_desc_new("*:Main", FALSE);
        method = mono_method_desc_search_in_image(desc, image);
    }

    if (!method) {
        dprintf("Unity: Target assembly does not have a valid entrypoint.\n");
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
