#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include "video.h"

#include "hook/procaddr.h"

#include "hooklib/dll.h"
#include "hooklib/path.h"

#include "util/dprintf.h"

static void* WINAPI hook_OpenSource(
    void* instance,
    const wchar_t* path,
    int videoApiIndex,
    bool useHardwareDecoding,
    bool generateTextureMips,
    bool hintAlphaChannel,
    bool useLowLatency,
    const wchar_t* forceAudioOutputDeviceName,
    bool useUnityAudio,
    bool forceResample,
    int sampleRate,
    const wchar_t** preferredFilter,
    uint32_t numFilters,
    int audioChannelMode,
    uint32_t sourceSampleRate,
    uint32_t sourceChannels
);

static void* (WINAPI *next_OpenSource)(
    void* instance,
    const wchar_t* path,
    int videoApiIndex,
    bool useHardwareDecoding,
    bool generateTextureMips,
    bool hintAlphaChannel,
    bool useLowLatency,
    const wchar_t* forceAudioOutputDeviceName,
    bool useUnityAudio,
    bool forceResample,
    int sampleRate,
    const wchar_t** preferredFilter,
    uint32_t numFilters,
    int audioChannelMode,
    uint32_t sourceSampleRate,
    uint32_t sourceChannels
);

static const struct hook_symbol av_pro_video_syms[] = {
    {
        .name = "OpenSource",
        .patch = hook_OpenSource,
        .link = (void **) &next_OpenSource,
    }
};

void av_pro_video_hook_init(struct video_config* video_cfg)
{
    assert(video_cfg != NULL);

    if (!video_cfg->enable) {
        return;
    }

    av_pro_video_hook_insert_hooks(NULL);

    dprintf("Video: hook enabled.\n");
}

void av_pro_video_hook_insert_hooks(HMODULE target)
{
    proc_addr_table_push(
        target,
        "AVProVideo.dll",
        av_pro_video_syms,
        _countof(av_pro_video_syms));
}

static void* WINAPI hook_OpenSource(
    void* instance,
    const wchar_t* path,
    int videoApiIndex,
    bool useHardwareDecoding,
    bool generateTextureMips,
    bool hintAlphaChannel,
    bool useLowLatency,
    const wchar_t* forceAudioOutputDeviceName,
    bool useUnityAudio,
    bool forceResample,
    int sampleRate,
    const wchar_t** preferredFilter,
    uint32_t numFilters,
    int audioChannelMode,
    uint32_t sourceSampleRate,
    uint32_t sourceChannels
) {
    wchar_t *trans;
    BOOL ok;

    ok = path_transform_w(&trans, path);

    if (!ok) {
        dprintf("AVProVideo: Path transformation error\n");
        return NULL;
    }

    return next_OpenSource(
        instance, trans ? trans : path, videoApiIndex, useHardwareDecoding, generateTextureMips,
        hintAlphaChannel, useLowLatency, forceAudioOutputDeviceName, useUnityAudio,
        forceResample, sampleRate, preferredFilter, numFilters, audioChannelMode,
        sourceSampleRate, sourceChannels);
}
