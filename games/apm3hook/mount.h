#pragma once

#include "board/aime-dll.h"
#include "platform/vfs.h"

struct mount_config {
    bool enable;
    bool delay;
};

void mount_hook_apply_hooks(HMODULE target);
void mount_hook_init(struct vfs_config* vfs_cfg, struct mount_config* mount_cfg);
