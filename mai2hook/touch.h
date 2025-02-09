#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

#include "hooklib/fdshark.h"
#include "hooklib/reg.h"
#include "hooklib/uart.h"
#include "mai2hook/mai2-dll.h"
#include "util/dprintf.h"
#include "util/dump.h"

struct touch_config
{
    bool enable_1p;
    bool enable_2p;
};

enum
{
    commandRSET = 0x45,  // E
    commandHALT = 0x4C,  // L
    commandSTAT = 0x41,  // A
    commandRatio = 0x72, // r
    commandSens = 0x6B,  // k
    req_start = 0x7b,    // {
    req_end = 0x7d,      // }
    res_start = 0x28,    // (
    res_end = 0x29,      // )
};

extern const char *sensor_map[34];
const char *sensor_to_str(uint8_t sensor);

HRESULT touch_hook_init(const struct touch_config *cfg);
static HRESULT touch_handle_irp(struct irp *irp);
static HRESULT touch_handle_irp_locked(struct irp *irp, struct uart *uart);

/* Called in mai2io to send touch data.
   Similar to chuni slider_res_auto_scan, but the host does not require periodic updates.
   Touch data is sent only when there is a change. */
static void touch_auto_scan(const uint8_t player, const uint8_t state[7]);