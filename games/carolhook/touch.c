#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "carolhook/carol-dll.h"
#include "carolhook/touch.h"

#include "hook/table.h"

#include "hooklib/uart.h"

#include "util/dprintf.h"
#include "util/dump.h"

/**
 * CMDS for touch thing
 * CX -> Calibrate Extend, preform callibration
 * MS -> Mode Stream, enters stream mode
 * R  -> Reset, resets the device
 * RD -> Reset Default, Resets the device to factory
 * Z  -> Null, keepalive command
 * NM -> Name, return the name of the device (not documented?)
 * OI -> Output Identity, output unique device identity, SC followed by 4 characters
 * UT -> Unit Type, returns controller unit type + status
 */

static HRESULT touch_handle_irp(struct irp *irp);
static HRESULT touch_handle_irp_locked(struct irp *irp);
static HRESULT touch_frame_decode(struct touch_req *dest, struct iobuf *iobuf);

static HRESULT handle_touch_ack_cmd(const struct touch_req *req);
static HRESULT handle_touch_name_cmd(const struct touch_req *req);
static HRESULT handle_touch_id_cmd(const struct touch_req *req);
static HRESULT handle_touch_unit_type_cmd(const struct touch_req *req);
static void touch_scan_auto(const bool is_pressed, const uint16_t mouse_x, const uint16_t mouse_y);

static CRITICAL_SECTION touch_lock;
static struct uart touch_uart;
static uint8_t touch_written_bytes[528];
static uint8_t touch_readable_bytes[528];
static bool should_stream = false;
static bool last_pressed;
static uint8_t last_x1;
static uint8_t last_x2;
static uint8_t last_y1;
static uint8_t last_y2;


HRESULT touch_hook_init(const struct touch_config *cfg)
{
    if (!cfg->enable) {
        return S_OK;
    }

    InitializeCriticalSection(&touch_lock);

    uart_init(&touch_uart, 1);
    touch_uart.written.bytes = touch_written_bytes;
    touch_uart.written.nbytes = sizeof(touch_written_bytes);
    touch_uart.readable.bytes = touch_readable_bytes;
    touch_uart.readable.nbytes = sizeof(touch_readable_bytes);

    dprintf("Touchscreen: Init\n");

    return iohook_push_handler(touch_handle_irp);
    return S_OK;
}

static HRESULT touch_handle_irp(struct irp *irp)
{
    HRESULT hr;

    assert(irp != NULL);

    if (!uart_match_irp(&touch_uart, irp)) {
        return iohook_invoke_next(irp);
    }

    EnterCriticalSection(&touch_lock);
    hr = touch_handle_irp_locked(irp);
    LeaveCriticalSection(&touch_lock);

    return hr;
}

static HRESULT touch_handle_irp_locked(struct irp *irp)
{
    struct touch_req req;
    HRESULT hr;

    assert(carol_dll.touch_init != NULL);

    if (irp->op == IRP_OP_OPEN) {
        dprintf("Touchscreen: Starting backend DLL\n");
        hr = carol_dll.touch_init();
        carol_dll.touch_start(touch_scan_auto);

        if (FAILED(hr)) {
            dprintf("Touchscreen: Backend DLL error: %X\n", (int) hr);

            return hr;
        }
    }

    hr = uart_handle_irp(&touch_uart, irp);

    if (FAILED(hr) || irp->op != IRP_OP_WRITE) {
        return hr;
    }

    for (;;) {
#if defined(LOG_CAROL_TOUCH)
        dprintf("Touchscreen: TX Buffer:\n");
        dump_iobuf(&touch_uart.written);
#endif
        hr = touch_frame_decode(&req, &touch_uart.written);

        if (FAILED(hr)) {
            dprintf("Touchscreen: Deframe Error: %X\n", (int) hr);

            return hr;
        }

        if (!strcmp("Z", (char *)req.cmd)) {
            hr = handle_touch_ack_cmd(&req);
        }
        else if (!strcmp("OI", (char *)req.cmd)) {
            hr = handle_touch_id_cmd(&req);
        }
        else if (!strcmp("UT", (char *)req.cmd)) {
            hr = handle_touch_unit_type_cmd(&req);
        }
        else if (!strcmp("NM", (char *)req.cmd)) {
            hr = handle_touch_name_cmd(&req);
        }
        else if (!strcmp("R", (char *)req.cmd)) {
            carol_dll.touch_stop();
            dprintf("Touch: Reset\n");
            hr = handle_touch_ack_cmd(&req);
        }
        else {
            dprintf("Touchscreen: Unhandled cmd %s\n", (char *)req.cmd);
            hr = handle_touch_ack_cmd(&req);
        }

        return hr;
    }
}

static HRESULT handle_touch_ack_cmd(const struct touch_req *req)
{
    return iobuf_write(&touch_uart.readable, "\0010\015", 3);
}

static HRESULT handle_touch_name_cmd(const struct touch_req *req)
{
    dprintf("Touch: Get Name\n");
    return iobuf_write(&touch_uart.readable, "\001AD1000\015", 15);
}

static HRESULT handle_touch_id_cmd(const struct touch_req *req)
{
    dprintf("Touch: Get ID\n");
    return iobuf_write(&touch_uart.readable, "\001AD1000\015", 8);
}

static HRESULT handle_touch_unit_type_cmd(const struct touch_req *req)
{
    dprintf("Touch: Get Unit Type\n");
    return iobuf_write(&touch_uart.readable, "\001AD****00\015", 8);
}

static void touch_scan_auto(const bool is_pressed, const uint16_t mouse_x, const uint16_t mouse_y)
{
    struct touch_auto_resp resp;
    uint16_t tmp_x;
    uint16_t tmp_y;
    bool flg = false;

    memset(&resp, 0, sizeof(resp));
    resp.touches[0].status |= 1 << 7;

    if (is_pressed) {
        resp.touches[0].status |= (1 << 7) | (1 << 6);
        resp.touches[0].touch_id = 1;
        tmp_x = mouse_x & 0x7FFF;
        tmp_y = mouse_y & 0x7FFF;

        resp.touches[0].x1 = tmp_x & 0x7F;
        resp.touches[0].x2 = (tmp_x >> 7) & 0x7F;
        resp.touches[0].y1 = tmp_y & 0x7F;
        resp.touches[0].y2 = (tmp_y >> 7) & 0x7F;

        flg = resp.touches[0].x1 != last_x1 || resp.touches[0].x2 != last_x2 || resp.touches[0].y1 != last_y1 || resp.touches[0].y2 != last_y2;

#if 0
        if (flg)
            dprintf("Touch: Mouse down! x %02X %02X y: %02X %02X\n", resp.touches[0].x1, resp.touches[0].x2, resp.touches[0].y1, resp.touches[0].y2);
#endif


        last_x1 = resp.touches[0].x1;
        last_x2 = resp.touches[0].x2;
        last_y1 = resp.touches[0].y1;
        last_y2 = resp.touches[0].y2;

    } else if (last_pressed) {
        resp.touches[0].x1 = last_x1;
        resp.touches[0].x2 = last_x2;
        resp.touches[0].y1 = last_y1;
        resp.touches[0].y2 = last_y2;
    }

    last_pressed = is_pressed;

    EnterCriticalSection(&touch_lock);
    iobuf_write(&touch_uart.readable, &resp, sizeof(resp));
    LeaveCriticalSection(&touch_lock);

#if defined(LOG_CAROL_TOUCH)
    dprintf("Touch: RX Buffer: (pos %08x)\n", (uint32_t)touch_uart.readable.pos);
    dump_iobuf(&touch_uart.readable);
#endif
}

/* Decodes the response into a struct that's easier to work with. */
static HRESULT touch_frame_decode(struct touch_req *dest, struct iobuf *iobuf)
{
    dest->sync = iobuf->bytes[0];
    memset(dest->cmd, 0, sizeof(dest->cmd));
    size_t data_len = 0;

    for (int i = 1; i < 255; i++) {
        if (iobuf->bytes[i] == 0x0D) { break; }
        dest->cmd[i - 1] = iobuf->bytes[i];
        data_len++;
    }

    dest->tail = iobuf->bytes[data_len + 1];
    dest->data_len = data_len;

    iobuf->pos = 0;

    if (dest->sync != 1 || dest->tail != 0x0D) {
        dprintf("Touch: Data recieve error, sync: 0x%02X (expected 0x01) tail: 0x%02X (expected 0x0D)", dest->sync, dest->tail);
        return E_FAIL;
    }

    return S_OK;
}
