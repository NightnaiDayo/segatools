#pragma once

#include "board/aime-dll.h"
#include "config.h"

void mount_hook_apply_hooks(HMODULE module);
void mount_hook_init(struct vfs_config* vfs_cfg, struct mount_config* mount_cfg);
