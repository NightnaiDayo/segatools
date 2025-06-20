#include "board/aime-dll.h"

#include "config.h"
#include "mount.h"

#include <assert.h>
#include <pathcch.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>

#include "hooklib/path.h"
#include "hook/procaddr.h"
#include "hook/table.h"

#include "util/dprintf.h"

/* API hooks */

static bool hook_ApmSys_mountVhd(const wchar_t* vhdOriginalPath, const wchar_t* vhdPatchPath, char mountDriveLetter);
static bool hook_ApmSys_mountFscrypt(const wchar_t* fscryptImagePath, const wchar_t* mountFolderPath, const wchar_t* subGameId);
static bool hook_ApmSys_unmountVhd(char mountDriveLetter);
static bool hook_ApmSys_unmountFscrypt(const wchar_t* mountFolderPath);

static const struct hook_symbol mount_hooks[] = {
    {
        .name   = "ApmSys_mountVhd",
        .patch  = hook_ApmSys_mountVhd,
        .ordinal = 2,
    },
    {
        .name   = "ApmSys_unmountVhd",
        .patch  = hook_ApmSys_unmountVhd,
        .ordinal = 4,
    },
    {
        .name   = "ApmSys_mountFscrypt",
        .patch  = hook_ApmSys_mountFscrypt,
        .ordinal = 1,
    },
    {
        .name   = "ApmSys_unmountFscrypt",
        .patch  = hook_ApmSys_unmountFscrypt,
        .ordinal = 3,
    },
};

static struct vfs_config* vcfg;
static struct mount_config* mcfg;

static wchar_t game_directory[MAX_PATH];

void mount_hook_init(struct vfs_config* vfs_cfg, struct mount_config* mount_cfg) {

    assert(vfs_cfg != NULL);
    assert(mount_cfg != NULL);

    vcfg = vfs_cfg;
    mcfg = mount_cfg;

    if (!mcfg->enable) {
        return;
    }

    dprintf("Mount: Hook enabled\n");
}

void mount_hook_apply_hooks(HMODULE module) {
    if (!mcfg->enable) {
        return;
    }

    proc_addr_table_push(module, "apmmount.dll", mount_hooks, _countof(mount_hooks));
}


bool hook_ApmSys_mountFscrypt(const wchar_t* fscryptImagePath, const wchar_t* mountFolderPath, const wchar_t* subGameId) {
    dprintf("Mount: Mount FsCrypt (%ls, %ls, %ls)\n", fscryptImagePath, mountFolderPath, subGameId);

    wcscpy_s(game_directory, MAX_PATH, fscryptImagePath);
    PathCchRemoveFileSpec(game_directory, MAX_PATH);
    wnsprintfW(game_directory, MAX_PATH, L"%s\\App", game_directory);

    wchar_t *trans;
    BOOL ok;

    ok = path_transform_w(&trans, game_directory);

    if (!ok) {
        dprintf("Mount: Path transformation error\n");
        return 1;
    }

    if (trans != NULL) {
        wcscpy_s(game_directory, MAX_PATH, trans);
    }

    free(trans);

    dprintf("Mount: Target Path: %ls\n", game_directory);

    return 0;
}

bool hook_ApmSys_mountVhd(const wchar_t* vhdOriginalPath, const wchar_t* vhdPatchPath, char mountDriveLetter) {
    dprintf("Mount: Mount VHD (%ls, %ls, %c)\n", vhdOriginalPath, vhdPatchPath, mountDriveLetter);

    if (game_directory[0] == '\0') {
        dprintf("Mount: Target directory is unset!\n");
        return 1;
    }

    // Game drive
    wchar_t device[3];
    device[0] = mountDriveLetter;
    device[1] = ':';
    device[2] = '\0';

    dprintf("Mount: Mapping %ls to %ls\n", device, game_directory);
    if (!DefineDosDeviceW(0, device, game_directory)) {
        dprintf("DefineDosDevice failed: %lx\n", GetLastError());
        return 1;
    }

    // APM root drive
    device[0] = 'X';

    wchar_t self_directory[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, self_directory);

    dprintf("Mount: Mapping %ls to %ls\n", device, self_directory);
    if (!DefineDosDeviceW(0, device, self_directory)) {
        dprintf("DefineDosDevice failed: %lx\n", GetLastError());
        return 1;
    }

    if (mcfg->delay) {
        Sleep(1500);
    }
    return 0;
}
bool hook_ApmSys_unmountVhd(char mountDriveLetter) {
    dprintf("Mount: Unmount VHD (%c)\n", mountDriveLetter);

    char device[3];
    device[0] = mountDriveLetter;
    device[1] = ':';
    device[2] = '\0';

    // Game drive
    if (!DefineDosDevice(DDD_REMOVE_DEFINITION, device, NULL)) {
        dprintf("DefineDosDevice failed: %lx\n", GetLastError());
        return 1;
    }

    // APM root drive
    device[0] = 'X';
    if (!DefineDosDevice(DDD_REMOVE_DEFINITION, device, NULL)) {
        dprintf("DefineDosDevice failed: %lx\n", GetLastError());
        return 1;
    }
    return 0;
}

void CALLBACK UnmountApmDrives(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow) {
    hook_ApmSys_unmountVhd('W');
    hook_ApmSys_unmountVhd('X');
}

bool hook_ApmSys_unmountFscrypt(const wchar_t* mountFolderPath) {
    dprintf("Mount: Unmount FsCrypt (%ls)\n", mountFolderPath);
    memset(game_directory, 0, MAX_PATH * sizeof(wchar_t));
    return 0;
}