#pragma once

#include <windows.h>

struct video_config {
    bool enable;
};

void av_pro_video_hook_init(struct video_config* video_cfg);
void av_pro_video_hook_insert_hooks(HMODULE target);
