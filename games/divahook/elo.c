/*
    Elo Touchsystems 2701 (Sega 838-14772) touchscreen controller emulator

    This board only supports single-touch.

    References:

    Elo 'SmartSet' protocol
    https://archive.org/details/manualzilla-id-5973842

    Credits:

    emihiok
*/

#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "board/elo-cmd.h"
#include "board/elo-frame.h"

#include "divahook/elo.h"
#include "divahook/diva-dll.h"

#include "hook/iobuf.h"
#include "hook/iohook.h"
#include "hook/table.h"

#include "hooklib/uart.h"

#include "util/dprintf.h"
#include "util/dump.h"

static HRESULT elo_handle_irp(struct irp *irp);
static HRESULT elo_handle_irp_locked(struct irp *irp);

static HRESULT elo_req_dispatch(const union elo_packet_any *req);
static HRESULT elo_req_acknowledge(void);
static HRESULT elo_req_reset(const struct elo_packet_reset *req);
static HRESULT elo_query_parameter(const struct elo_packet_parameter *req);
static HRESULT elo_set_parameter(const struct elo_packet_parameter *req);

static HRESULT elo_send_acknowledge(void);
static void elo_send_touch(
        const uint8_t status,
        const uint16_t x,
        const uint16_t y,
        const uint8_t id);

static void elo_set_error(uint8_t error_code);
static void elo_clear_error(void);

static CRITICAL_SECTION elo_lock;
static struct uart elo_uart;
static uint8_t elo_written_bytes[520];
static uint8_t elo_readable_bytes[520];

static uint8_t elo_last_error[4];
static bool elo_acknowledge_required;
static bool elo_error_report_required;
static bool elo_touch_started;

/* Cursor specific API hooks */

static HCURSOR hook_SetCursor(HCURSOR hCursor);
static HCURSOR (*next_SetCursor)(HCURSOR hCursor);

static const struct hook_symbol cursor_syms[] = {
    {
        .name  = "SetCursor",
        .patch = hook_SetCursor,
        .link  = (void **) &next_SetCursor
    },
};

HRESULT elo_hook_init(
        const struct elo_config *cfg,
        unsigned int port_no)
{
    assert(cfg != NULL);

    if (!cfg->enable) {
        return S_FALSE;
    }

    if (cfg->port_no != 0) {
        port_no = cfg->port_no;
    }

    hook_table_apply(
        NULL,
        "user32.dll",
        cursor_syms,
        _countof(cursor_syms));

    InitializeCriticalSection(&elo_lock);

    uart_init(&elo_uart, port_no);
    elo_uart.baud.BaudRate = 9600;
    elo_uart.written.bytes = elo_written_bytes;
    elo_uart.written.nbytes = sizeof(elo_written_bytes);
    elo_uart.readable.bytes = elo_readable_bytes;
    elo_uart.readable.nbytes = sizeof(elo_readable_bytes);

    memset(elo_last_error, ELO_ERR_NONE, sizeof(elo_last_error));
    elo_acknowledge_required = false;
    elo_error_report_required = false;
    elo_touch_started = false;

    return iohook_push_handler(elo_handle_irp);
}

static HCURSOR hook_SetCursor(HCURSOR hCursor)
{
    HCURSOR fake_cursor;

    if (hCursor)
        return next_SetCursor(hCursor);

    fake_cursor = LoadCursorA(NULL, IDC_CROSS);
    return next_SetCursor(fake_cursor);
}


static HRESULT elo_handle_irp(struct irp *irp)
{
    HRESULT hr;

    assert(irp != NULL);

    if (!uart_match_irp(&elo_uart, irp)) {
        return iohook_invoke_next(irp);
    }

    EnterCriticalSection(&elo_lock);
    hr = elo_handle_irp_locked(irp);
    LeaveCriticalSection(&elo_lock);

    return hr;
}

static HRESULT elo_handle_irp_locked(struct irp *irp)
{
    union elo_packet_any req;
    struct iobuf req_iobuf;
    HRESULT hr;

    assert(diva_dll.touch_init != NULL);

    if (irp->op == IRP_OP_OPEN) {
        dprintf("Elo Touch: Starting DIVA Touch backend\n");
        hr = diva_dll.touch_init();

        if (FAILED(hr)) {
            dprintf("Elo Touch: Backend error, touchscreen disconnected: %x\n",
                    (int) hr);

            return hr;
        }

        dprintf("Elo Touch: Start touch thread\n");
        diva_dll.touch_start(elo_send_touch);
        elo_touch_started = true;
    }

    hr = uart_handle_irp(&elo_uart, irp);

    if (FAILED(hr) || irp->op != IRP_OP_WRITE) {
        return hr;
    }

    for (;;) {
        if (elo_acknowledge_required) {
            hr = elo_send_acknowledge();

            if (FAILED(hr)) {
                dprintf("Elo Touch: Acknowledge failed: %x\n", (int) hr);
            }
        }

#if 0
        dprintf("Elo Touch DEBUG: TX Buffer:\n");
        dump_iobuf(&elo_uart.written);
#endif

        req_iobuf.bytes = req.bytes;
        req_iobuf.nbytes = sizeof(req.bytes);
        req_iobuf.pos = 0;

        hr = elo_frame_decode(&req_iobuf, &elo_uart.written);

        if (hr != S_OK) {
            if (hr == HRESULT_FROM_WIN32(ERROR_CRC)) {
                elo_set_error(ELO_ERR_BAD_INPUT_CHECKSUM);
            }

            if (FAILED(hr)) {
                dprintf("Elo Touch: Deframe error: %x\n", (int) hr);
            }

            return hr;
        }

#if 0
        dprintf("Elo Touch DEBUG: Deframe Buffer:\n");
        dump_iobuf(&req_iobuf);
#endif
        hr = elo_req_dispatch(&req);

        if (FAILED(hr)) {
            dprintf("Elo Touch: Processing error: %x\n", (int) hr);
        }

        if (SUCCEEDED(hr)) {
            if (!elo_touch_started) {
                dprintf("Elo Touch: Restart touch reports\n");
                diva_dll.touch_start(elo_send_touch);
                elo_touch_started = true;
            }
        }
    }
}

static HRESULT elo_req_dispatch(const union elo_packet_any *req)
{
    switch (req->hdr.cmd) {
    case ELO_CMD_QUERY_ACKNOWLEDGE:
        return elo_req_acknowledge();

    case ELO_CMD_SET_RESET:
        return elo_req_reset(&req->reset);

    case ELO_CMD_QUERY_PARAMETER:
        return elo_query_parameter(&req->param);

    case ELO_CMD_SET_PARAMETER:
        return elo_set_parameter(&req->param);

    case ELO_CMD_ACKNOWLEDGE:
        dprintf("Elo Touch: Invalid command, ACK cannot be set\n");
        elo_set_error(ELO_ERR_SET_UNAVAILABLE);

        return E_INVALIDARG;

    case 'r':
        dprintf("Elo Touch: Invalid command, Reset cannot be queried\n");
        elo_set_error(ELO_ERR_QUERY_UNAVAILABLE);

        return E_INVALIDARG;

    default:
        dprintf("Elo Touch: Unhandled command %02x\n", req->hdr.cmd);
        elo_set_error(ELO_ERR_ILLEGAL_COMMAND);

        return S_OK;
    }
}

static HRESULT elo_req_acknowledge(void)
{
    dprintf("Elo Touch: Query acknowledge\n");

    elo_acknowledge_required = true;

    return S_OK;
}

static HRESULT elo_req_reset(const struct elo_packet_reset *req)
{
    if (req->r_type == '0') {
        dprintf("Elo Touch: Hard reset\n");
        elo_clear_error();

        /* IO DLL worker thread might attempt to invoke the callback (which needs
           to take elo_lock, which we are currently holding) before noticing that
           it needs to shut down. Unlock here so that we don't deadlock in that
           situation. */

        LeaveCriticalSection(&elo_lock);
        dprintf("Elo Touch: Stop touch reports\n");
        diva_dll.touch_stop();
        elo_touch_started = false;
        EnterCriticalSection(&elo_lock);

        /* No response or ack whatsoever as controller fully reboots.
           Game will wait approx. 5 seconds before sending the next command */
        return S_OK;
    } else if (req->r_type == '1') {
        dprintf("Elo Touch: Soft reset\n");
    } else {
        /* Type '2' (reset to NVRAM defaults) is only available on COACh IV
           controllers, which the 2701 does not have. */
        dprintf("Elo Touch: Warning -- Unknown reset type %02x\n", req->r_type);
    }

    union elo_packet_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.lead = ELO_FRAME_LEAD;
    resp.hdr.cmd = ELO_CMD_SET_RESET;

    /* Response only, message is not acknowledged */
    return elo_frame_encode(&elo_uart.readable, &resp, sizeof(resp));
}

static HRESULT elo_query_parameter(const struct elo_packet_parameter *req)
{
    dprintf("Elo Touch: Query parameters\n");

    struct elo_packet_parameter resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.lead = ELO_FRAME_LEAD;
    resp.hdr.cmd = ELO_CMD_SET_PARAMETER;

    resp.io = '0';          // Serial
    resp.ser1 = 0b00000110; // 9600 bps, 8-N-1
    resp.ser2 = 0b00000100; // Hardware handshaking enabled

    elo_acknowledge_required = true;

    return elo_frame_encode(&elo_uart.readable, &resp, sizeof(resp));
}

static HRESULT elo_set_parameter(const struct elo_packet_parameter *req)
{
    dprintf("Elo Touch: Set parameters\n");
    dprintf("Elo Touch: --- Begin Parameter Receive ---\n");

    bool bad_bit_flag = false;
    bool unimpl_flag = false;

    if (req->io == '0') {
        dprintf("Elo Touch: Param: Serial interface\n");
    } else {
        dprintf("Elo Touch: Param: Unknown IO preset: %02x\n", req->io);
        unimpl_flag = true;
        goto end;
    }

    switch (req->ser1 & 0b111) {
    case 0b101:
        dprintf("Elo Touch: Param: Baudrate 4800bps\n");
        elo_uart.baud.BaudRate = 4800;
        break;
    case 0b110:
        dprintf("Elo Touch: Param: Baudrate 9600bps\n");
        elo_uart.baud.BaudRate = 9600;
        break;
    case 0b111:
        dprintf("Elo Touch: Param: Baudrate 19200bps\n");
        elo_uart.baud.BaudRate = 19200;
        break;
    default:
        dprintf("Elo Touch: Param: Unknown Serial1 preset: %02x\n", req->ser1);
        unimpl_flag = true;
        goto end;
    }

    for (int i = 0 ; i < 8 ; i++) {
        bool set = (req->ser2) & (1 << i);
        switch (i) {
        case 0:
            if (set) dprintf("Elo Touch: Param: Checksum required\n");
            break;
        case 1:
            if (set) dprintf("Elo Touch: Param: Software handshaking "
                            "enabled\n");
            break;
        case 2:
            if (set) {
                dprintf("Elo Touch: Param: Hardware handshaking enabled\n");
                elo_uart.handflow.ControlHandShake =
                        SERIAL_DTR_CONTROL | SERIAL_DTR_HANDSHAKE;
                elo_uart.handflow.FlowReplace =
                        SERIAL_RTS_CONTROL | SERIAL_RTS_HANDSHAKE;
            }
            break;
        case 3:
            if (set) dprintf("Elo Touch: Param: Invert hardware "
                            "handshaking\n");
            break;
        /* Reserved bits should not be set */
        case 4:
        case 5:
        case 6:
            if (set) bad_bit_flag = true;
            break;
        case 7:
            if (set) dprintf("Elo Touch: Param: Full duplex\n");
            break;
        default:
            break;
        }
    }

end:
    dprintf("Elo Touch: --- End Parameter Receive ---\n");

    elo_acknowledge_required = true;

    if (bad_bit_flag) {
        elo_set_error(ELO_ERR_BAD_INPUT_PACKET);

        return E_INVALIDARG;
    } else if (unimpl_flag) {
        elo_set_error(ELO_ERR_BAD_SERIAL_SETUP);

        return E_NOTIMPL;
    }

    return S_OK;
}

static HRESULT elo_send_acknowledge(void)
{
    dprintf("Elo Touch: Acknowledge\n");

    struct elo_packet_acknowledge resp;
    HRESULT hr;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.lead = ELO_FRAME_LEAD;
    resp.hdr.cmd = ELO_CMD_ACKNOWLEDGE;

    for (int i = 0 ; i < sizeof(resp.error_code) ; i++) {
        resp.error_code[i] = elo_last_error[i];
    }

    hr = elo_frame_encode(&elo_uart.readable, &resp, sizeof(resp));

    elo_acknowledge_required = false;

    return hr;
}

static void elo_send_touch(
        const uint8_t status,
        const uint16_t x,
        const uint16_t y,
        const uint8_t id)
{
    if (!elo_touch_started || !status || !id) return;
    if (id > 1) return;

    /* Generate "IntelliTouch" packet */
    struct elo_packet_touch resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.lead = ELO_FRAME_LEAD;
    resp.hdr.cmd = ELO_CMD_TOUCH;

    /* IntelliTouch touch events always have MSB (Z-Axis Valid) set */
    switch (status) {
    case DIVA_IO_TOUCH_DOWN:
        resp.status |= (1 << 7) | 1;
        break;
    case DIVA_IO_TOUCH_STREAM:
        resp.status |= (1 << 7) | (1 << 1);
        break;
    case DIVA_IO_TOUCH_LIFTOFF:
        resp.status |= (1 << 7) | (1 << 2);
        break;
    default:
        /* No touch events occurred..? */
        return;
    }

    /* Touch event detected, report coordinates */
    if ((resp.status & 0b111) != 0) {
        /* Max resolution: 4095 x 4095 */
        uint16_t x_restrict = x & 0x0FFF;
        uint16_t y_restrict = y & 0x0FFF;

        resp.x_low = x_restrict & UINT8_MAX;
        resp.x_high = x_restrict >> 8;
        resp.y_low = y_restrict & UINT8_MAX;
        resp.y_high = y_restrict >> 8;
        resp.z_low = UINT8_MAX; // Ignore pressure for now
        resp.z_high = 0; // Unused
    }

    if (elo_error_report_required) {
        resp.status |= 1 << 4;
    }

#if 0
    dprintf("Elo Touch DEBUG: STATUS:%02x, X LOW:%02x, X HIGH:%02x, "
            "Y LOW:%02x, Y HIGH:%02x, Z LOW:%02x, Z HIGH:%02x\n",
            resp.status, resp.x_low, resp.x_high, resp.y_low, resp.y_high,
            resp.z_low, resp.z_high);
#endif

    EnterCriticalSection(&elo_lock);
    elo_frame_encode(&elo_uart.readable, &resp, sizeof(resp));
    LeaveCriticalSection(&elo_lock);

    elo_error_report_required = false;
}

static void elo_set_error(uint8_t error_code)
{
    bool empty_slot_found = false;

    for (int i = 0 ; i < 4 && empty_slot_found == false ; i++) {
        if (elo_last_error[i] == ELO_ERR_NONE) {
            elo_last_error[i] = error_code;
            empty_slot_found = true;
        }
    }

    /* All error slots full, overwrite first slot */
    if (!empty_slot_found) {
        elo_last_error[0] = error_code;
    }

    elo_error_report_required = true;
}

static void elo_clear_error(void)
{
    memset(&elo_last_error, ELO_ERR_NONE, sizeof(elo_last_error));
}
