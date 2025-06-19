#pragma once

#include <windows.h>

#include "platform/amvideo.h"
#include "platform/clock.h"
#include "platform/dns.h"
#include "platform/epay.h"
#include "platform/hwmon.h"
#include "platform/hwreset.h"
#include "platform/misc.h"
#include "platform/netenv.h"
#include "platform/nusec.h"
#include "platform/pcbid.h"
#include "platform/vfs.h"
#include "platform/system.h"

struct platform_config {
    struct amvideo_config amvideo;
    struct clock_config clock;
    struct dns_config dns;
    struct epay_config epay;
    struct hwmon_config hwmon;
    struct hwreset_config hwreset;
    struct misc_config misc;
    struct pcbid_config pcbid;
    struct netenv_config netenv;
    struct nusec_config nusec;
    struct vfs_config vfs;
    struct system_config system;
};

HRESULT platform_hook_init(
        const struct platform_config *cfg,
        const char *game_id,
        const char *platform_id,
        HMODULE redir_mod);
