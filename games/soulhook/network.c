#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <ws2tcpip.h>

#include "hook/table.h"

#include "hooklib/dll.h"

#include "util/dprintf.h"

#include "platform/platform.h"

#include "network.h"

static int hook_bind(SOCKET s, const struct sockaddr* addr, int namelen);
static int (*next_bind)(SOCKET s, const struct sockaddr* addr, int namelen);
static int local_ip;

static const struct hook_symbol network_syms[] = {{
    .name       = "bind",
    .ordinal    = 2,
    .patch      = hook_bind,
    .link       = (void**)&next_bind,
}};

void network_hook_init(struct platform_config* config)
{
    if (config->netenv.enable) {
        dprintf("Network: Hooks enabled.\n");

        local_ip = htonl(config->nusec.subnet | config->netenv.addr_suffix);

        hook_table_apply(
            NULL,
            "ws2_32.dll",
            network_syms,
            _countof(network_syms));
    }
}

static int hook_bind(SOCKET s, const struct sockaddr* addr, int namelen) {
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;

        if (!memcmp(&addr_in->sin_addr, &local_ip, 4)) {
            ZeroMemory(&addr_in->sin_addr, sizeof(struct in_addr));
        }
    }

    return next_bind(s, addr, namelen);
}
