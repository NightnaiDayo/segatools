#pragma once

#include <windows.h>
#include <stdbool.h>

HRESULT createprocess_push_hook_w(const wchar_t *name, const wchar_t *head, const wchar_t *tail, bool replace_all, bool replace_paths);
HRESULT createprocess_push_hook_a(const char *name, const char *head, const char *tail, bool replace_all, bool replace_paths);
void createprocess_hook_apply_hooks(HMODULE mod);

struct process_hook_sym_w {
    const wchar_t *name;
    const wchar_t *head;
    const wchar_t *tail;
    bool replace_all;
    bool replace_paths;
};

struct process_hook_sym_a {
    const char *name;
    const char *head;
    const char *tail;
    bool replace_all;
    bool replace_paths;
};