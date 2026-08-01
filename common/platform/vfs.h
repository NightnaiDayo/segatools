#pragma once

#include <windows.h>

#include <stdbool.h>
#include <stddef.h>

#define MAX_REDIRECTIONS 8

struct vfs_config {
    bool enable;
    wchar_t amfs[MAX_PATH];
    wchar_t appdata[MAX_PATH];
    wchar_t option[MAX_PATH];
    wchar_t redirections_from[MAX_REDIRECTIONS][MAX_PATH];
    int redirections_from_len[MAX_REDIRECTIONS];
    wchar_t redirections_to[MAX_REDIRECTIONS][MAX_PATH];
    bool allowAmfsDownloads;
};

HRESULT vfs_hook_init(const struct vfs_config *config, const char* game_id);
