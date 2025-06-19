/*
    Force Feedback Board (FFB)

    This board is used by many SEGA games to provide force feedback to the player.
    It is driven by the game software over a serial connection and is used by many
    games such as SEGA World Drivers Championship, Initial D Arcade, ...

    Part number in schematics is "838-15069 MOTOR DRIVE BD RS232/422 Board".

    Some observations:
    The maximal strength for any effect is 127, except Damper which maxes out at 40.
    The period for rumble effects is in the range 0-40.
*/

#include "board/ffb.h"

#include <assert.h>
#include <stdint.h>
#include <windows.h>

#include "hook/iohook.h"
#include "hooklib/uart.h"
#include "util/dprintf.h"
#include "util/dump.h"


// request format:
// 0x?? - sync + command
// 0x?? - direction/additional command
// 0x?? - strength
// 0x?? - checksum (sum of everything except the sync byte)

enum {
    FFB_CMD_TOGGLE = 0x80,
    FFB_CMD_CONSTANT_FORCE = 0x84,
    FFB_CMD_RUMBLE = 0x85,
    FFB_CMD_DAMPER = 0x86,
};

struct ffb_hdr {
    uint8_t cmd;
};

union ffb_req_any {
    struct ffb_hdr hdr;
    uint8_t bytes[3];
};

static HRESULT ffb_handle_irp(struct irp *irp);

static HRESULT ffb_req_dispatch(const union ffb_req_any *req);
static HRESULT ffb_req_toggle(const uint8_t *bytes);
static HRESULT ffb_req_constant_force(const uint8_t *bytes);
static HRESULT ffb_req_rumble(const uint8_t *bytes);
static HRESULT ffb_req_damper(const uint8_t *bytes);

static const struct ffb_ops *ffb_ops;
static struct uart ffb_uart;

static bool ffb_started;
static HRESULT ffb_start_hr;
static uint8_t ffb_written[4];
static uint8_t ffb_readable[4];

/* Static variables to store maximum strength values */
static uint8_t max_constant_force = 0;
static uint8_t max_rumble = 0;
static uint8_t max_period = 0;
static uint8_t max_damper = 0;

HRESULT ffb_hook_init(
        const struct ffb_config *cfg,
        const struct ffb_ops *ops,
        unsigned int port_no)
{
    assert(cfg != NULL);
    assert(ops != NULL);

    if (!cfg->enable) {
        return S_FALSE;
    }

    ffb_ops = ops;

    uart_init(&ffb_uart, port_no);
    ffb_uart.written.bytes = ffb_written;
    ffb_uart.written.nbytes = sizeof(ffb_written);
    ffb_uart.readable.bytes = ffb_readable;
    ffb_uart.readable.nbytes = sizeof(ffb_readable);

    dprintf("FFB: hook enabled.\n");

    return iohook_push_handler(ffb_handle_irp);
}

static HRESULT ffb_handle_irp(struct irp *irp)
{
    HRESULT hr;

    assert(irp != NULL);

    if (!uart_match_irp(&ffb_uart, irp)) {
        return iohook_invoke_next(irp);
    }

    hr = uart_handle_irp(&ffb_uart, irp);

    if (FAILED(hr) || irp->op != IRP_OP_WRITE) {
        return hr;
    }

    assert(&ffb_uart.written != NULL);
    assert(ffb_uart.written.bytes != NULL || ffb_uart.written.nbytes == 0);
    assert(ffb_uart.written.pos <= ffb_uart.written.nbytes);

    // dprintf("FFB TX:\n");

    hr = ffb_req_dispatch((const union ffb_req_any *) ffb_uart.written.bytes);

    if (FAILED(hr)) {
        dprintf("FFB: Processing error: %x\n", (int)hr);
    }

    // dump_iobuf(&ffb_uart.written);
    ffb_uart.written.pos = 0;

    return hr;
}

static HRESULT ffb_req_dispatch(const union ffb_req_any *req)
{
    switch (req->hdr.cmd) {
    case FFB_CMD_TOGGLE:
        return ffb_req_toggle(req->bytes);
    case FFB_CMD_CONSTANT_FORCE:
        return ffb_req_constant_force(req->bytes);
    case FFB_CMD_RUMBLE:
        return ffb_req_rumble(req->bytes);
    case FFB_CMD_DAMPER:
        return ffb_req_damper(req->bytes);

    /* There are some test mode specfic commands which doesn't seem to be used in
       game at all. The same is true for the initialization phase. */

    default:
        dprintf("FFB: Unhandled command %02x\n", req->hdr.cmd);

        return S_OK;
    }
}

static HRESULT ffb_req_toggle(const uint8_t *bytes)
{
    uint8_t activate = bytes[2];

    if (activate == 0x01) {
        dprintf("FFB: Activated\n");
    } else {
        dprintf("FFB: Deactivated\n");
    }

    if (ffb_ops->toggle != NULL) {
        ffb_ops->toggle(activate == 0x01);
    }

    return S_OK;
}

static HRESULT ffb_req_constant_force(const uint8_t *bytes)
{
    // dprintf("FFB: Constant force\n");

    uint8_t direction = bytes[1];
    uint8_t force = bytes[2];

    if (direction == 0x0) {
        // Right
        force = 128 - force;
    }

    // Update max strength if the current force is greater
    if (force > max_constant_force) {
        max_constant_force = force;
    }

    // dprintf("FFB: Constant Force Strength: %d (Max: %d)\n", force, max_constant_force);
    if (ffb_ops->constant_force != NULL) {
        ffb_ops->constant_force(direction, force);
    }

    return S_OK;
}

static HRESULT ffb_req_rumble(const uint8_t *bytes)
{
    // dprintf("FFB: Rumble\n");

    uint8_t force = bytes[1];
    uint8_t period = bytes[2];

    // Update max strength if the current force is greater
    if (force > max_rumble) {
        max_rumble = force;
    }

    if (period > max_period) {
        max_period = period;
    }

    // dprintf("FFB: Rumble Period: %d (Max %d), Strength: %d (Max: %d)\n", period, max_period, force, max_rumble);
    if (ffb_ops->rumble != NULL) {
        ffb_ops->rumble(force, period);
    }

    return S_OK;
}

static HRESULT ffb_req_damper(const uint8_t *bytes)
{
    // dprintf("FFB: Damper\n");

    uint8_t force = bytes[2];

    // Update max strength if the current force is greater
    if (force > max_damper) {
        max_damper = force;
    }

    // dprintf("FFB: Damper Strength: %d (Max: %d)\n", force, max_damper);
    if (ffb_ops->damper != NULL) {
        ffb_ops->damper(force);
    }

    return S_OK;
}
