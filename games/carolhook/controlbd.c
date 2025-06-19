#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "hook/iobuf.h"
#include "hook/iohook.h"

#include "carolhook/carol-dll.h"
#include "carolhook/controlbd.h"

#include "hooklib/uart.h"

#include "util/dprintf.h"
#include "util/dump.h"

static HRESULT controlbd_handle_irp(struct irp *irp);
static HRESULT controlbd_handle_irp_locked(struct irp *irp);
static HRESULT controlbd_frame_decode(struct controlbd_req_any *dest, struct iobuf *src);
static HRESULT controlbd_set_header(struct controlbd_resp_hdr *resp, uint8_t cmd, uint8_t len);
static uint8_t calc_checksum(void *data, size_t len);

static HRESULT controlbd_req_dispatch(const struct controlbd_req_any *req);
static HRESULT controlbd_req_ack_any(uint8_t cmd);
static HRESULT controlbd_req_reset(void);
static HRESULT controlbd_req_get_board_info(void);
static HRESULT controlbd_req_firmware_checksum(void);
static HRESULT controlbd_req_polling(const struct controlbd_req_any *req);

const uint8_t CONTROLBD_SYNC_BYTE = 0xE0;

static CRITICAL_SECTION controlbd_lock;
static struct uart controlbd_uart;
static uint8_t controlbd_written_bytes[520];
static uint8_t controlbd_readable_bytes[520];

HRESULT controlbd_hook_init(const struct controlbd_config *cfg)
{
    if (!cfg->enable) {
        return S_OK;
    }

    InitializeCriticalSection(&controlbd_lock);

    uart_init(&controlbd_uart, 12);
    controlbd_uart.written.bytes = controlbd_written_bytes;
    controlbd_uart.written.nbytes = sizeof(controlbd_written_bytes);
    controlbd_uart.readable.bytes = controlbd_readable_bytes;
    controlbd_uart.readable.nbytes = sizeof(controlbd_readable_bytes);

    dprintf("Control Board: Init\n");

    return iohook_push_handler(controlbd_handle_irp);
}

static HRESULT controlbd_handle_irp(struct irp *irp)
{
    HRESULT hr;

    assert(irp != NULL);

    if (!uart_match_irp(&controlbd_uart, irp)) {
        return iohook_invoke_next(irp);
    }

    EnterCriticalSection(&controlbd_lock);
    hr = controlbd_handle_irp_locked(irp);
    LeaveCriticalSection(&controlbd_lock);

    return hr;
}

static HRESULT controlbd_frame_decode(struct controlbd_req_any *req, struct iobuf *src)
{
    uint8_t data_len = 0;
    uint8_t checksum_pos = src->pos - 1;
    uint8_t calculated_checksum = 0;
    uint8_t checksum = 0;

    if (src->pos < 6) {
        dprintf("Control Board: Decode Error, request too short (pos is 0x%08X)\n", (int)src->pos);
        return SEC_E_BUFFER_TOO_SMALL;
    }

    req->hdr.sync = src->bytes[0];
    req->hdr.dest = src->bytes[1];
    req->hdr.src = src->bytes[2];
    req->hdr.len = src->bytes[3];
    req->hdr.cmd = src->bytes[4];
    data_len = req->hdr.len;
    src->pos -= 5;

    for (int i = 0; i < data_len; i++) {
        if (src->pos == 0) {
            break;
        }
        req->bytes[i] = src->bytes[i + 5];
        src->pos --;
    }

    checksum = src->bytes[checksum_pos];
    calculated_checksum = calc_checksum(req, checksum_pos);

    if (checksum != calculated_checksum) {
        dprintf("Control Board: Decode Error, checksum failure (expected 0x%02X, got 0x%02X)\n", calculated_checksum, checksum);
        return HRESULT_FROM_WIN32(ERROR_CRC);
    }

    return S_OK;
}

static HRESULT controlbd_handle_irp_locked(struct irp *irp)
{
    HRESULT hr;
    struct controlbd_req_any req;

    assert(carol_dll.controlbd_init != NULL);

    if (irp->op == IRP_OP_OPEN) {
        dprintf("Control Board: Starting backend DLL\n");
        hr = carol_dll.controlbd_init();

        if (FAILED(hr)) {
            dprintf("Control Board: Backend DLL error: 0X%X\n", (int) hr);

            return hr;
        }
    }

    hr = uart_handle_irp(&controlbd_uart, irp);

    if (FAILED(hr) || irp->op != IRP_OP_WRITE) {
        return hr;
    }

    for (;;) {
        if (controlbd_uart.written.bytes[0] == 0xE0) {
#if defined(LOG_CAROL_CONTROL_BD)
            dprintf("Control Board: TX Buffer:\n");
            dump_iobuf(&controlbd_uart.written);
#endif
            hr = controlbd_frame_decode(&req, &controlbd_uart.written);
            if (FAILED(hr)) {
                dprintf("Control Board: Deframe error: %x\n", (int) hr);
                return hr;
            }

            hr = controlbd_req_dispatch(&req);
            if (FAILED(hr)) {
                dprintf("Control Board: Dispatch Error: 0X%X\n", (int) hr);
                return hr;
            }
#if defined(LOG_CAROL_CONTROL_BD)
            dprintf("Control Board: RX Buffer:\n");
            dump_iobuf(&controlbd_uart.readable);
#endif
            controlbd_uart.written.pos = 0;
            return hr;
        }

        // The board has a LPC111x bootloader that gets ran over every boot
        // this is to account for that.
        char cmd[255];
        strcpy_s(cmd, 255, (char *)controlbd_uart.written.bytes);
        cmd[controlbd_uart.written.pos] = '\0';

        if (!strcmp(cmd, "?")) {
            dprintf("Control Board: Bootloader Hello\n");
        }
        else if (!strcmp(cmd, "Synchronized\r\n")) {
            iobuf_write(&controlbd_uart.readable, "Synchronized\r\nNG\r\n", 19);
            // Set this to OK instead of NG to do an update
        }
        else if (!strcmp(cmd, "12000\r\n")) {
            iobuf_write(&controlbd_uart.readable, "12000\r\nOK\r\n", 12);
        }
        else {
            // Everything other then the two commands above just want 0\r\n
            // appended to the request as the response. Given that it only checks sometimes,
            // it's safe to just run over the readable buffer every response.
            controlbd_uart.readable.pos = 0;
            cmd[controlbd_uart.written.pos] = '0';
            cmd[controlbd_uart.written.pos + 1] = '\r';
            cmd[controlbd_uart.written.pos + 2] = '\n';
            cmd[controlbd_uart.written.pos + 3] = '\0';
            // dprintf("Control Board: Return %s\n", cmd);
            iobuf_write(&controlbd_uart.readable, cmd, controlbd_uart.written.pos + 3);
        }

        controlbd_uart.written.pos = 0;
        return hr;
    }
}

static HRESULT controlbd_req_dispatch(const struct controlbd_req_any *req)
{
    switch (req->hdr.cmd) {
    case CONTROLBD_CMD_RESET:
        return controlbd_req_reset();

    case CONTROLBD_CMD_BDINFO:
        return controlbd_req_get_board_info();

    case CONTROLBD_CMD_FIRM_SUM:
        return controlbd_req_firmware_checksum();

    case CONTROLBD_CMD_TIMEOUT:
        dprintf("Control Board: Acknowledge Timeout\n");
        return controlbd_req_ack_any(req->hdr.cmd);

    case CONTROLBD_CMD_PORT_SETTING:
        dprintf("Control Board: Acknowledge Port Setting\n");
        return controlbd_req_ack_any(req->hdr.cmd);

    case CONTROLBD_CMD_INITIALIZE:
        dprintf("Control Board: Acknowledge Initialize\n");
        return controlbd_req_ack_any(req->hdr.cmd);

    case CONTROLBD_CMD_POLLING:
        return controlbd_req_polling(req);

    default:
        dprintf("Unhandled command 0x%02x\n", req->hdr.cmd);
        return controlbd_req_ack_any(req->hdr.cmd);
    }
}

static HRESULT controlbd_set_header(struct controlbd_resp_hdr *resp, uint8_t cmd, uint8_t len)
{
    resp->sync = CONTROLBD_SYNC_BYTE;
    resp->dest = 0x01;
    resp->src = 0x02;
    resp->len = len;
    resp->report = 0x01;
    resp->cmd = cmd;
    return S_OK;
}

static uint8_t calc_checksum(void *data, size_t len)
{
    uint8_t *stuff;
    stuff = data;
    uint8_t checksum = 0;
    uint16_t tmp = 0;

    for (int i = 1; i < len; i++) {
        tmp = checksum + stuff[i];
        checksum = tmp & 0xFF;
    }

    return checksum;
}

static HRESULT controlbd_req_reset(void)
{
    struct controlbd_resp_reset resp;

    dprintf("Control Board: Reset\n");

    controlbd_set_header(&resp.hdr, CONTROLBD_CMD_RESET, 2);
    resp.checksum = 0;

    // No data, just ack
    resp.checksum = calc_checksum(&resp, sizeof(resp));
    return iobuf_write(&controlbd_uart.readable, &resp, sizeof(resp));

}

static HRESULT controlbd_req_get_board_info(void)
{
    struct controlbd_resp_bdinfo resp;
    memset(&resp, 0, sizeof(resp));
    dprintf("Control Board: Get Board Info\n");
    controlbd_set_header(&resp.hdr, CONTROLBD_CMD_BDINFO, 21);

    resp.rev = 0x90;
    resp.bfr_size = 0x0001;
    resp.ack = 1;

    strcpy_s(resp.bd_no, sizeof(resp.bd_no), "15312   ");
    strcpy_s(resp.chip_no, sizeof(resp.chip_no), "6699 ");
    resp.chip_no[5] = 0xFF;
    resp.bd_no[8] = 0x0A;

    resp.checksum = calc_checksum(&resp, sizeof(resp));

    return iobuf_write(&controlbd_uart.readable, &resp, sizeof(resp));
}

static HRESULT controlbd_req_firmware_checksum(void)
{
    struct controlbd_resp_fw_checksum resp;
    memset(&resp, 0, sizeof(resp));
    dprintf("Control Board: Get Firmware Checksum\n");
    controlbd_set_header(&resp.hdr, CONTROLBD_CMD_FIRM_SUM, 5);

    resp.ack = 1;
    resp.fw_checksum = 0x1b36; // This could change with an update... oh well
    resp.checksum = calc_checksum(&resp, sizeof(resp));

    return iobuf_write(&controlbd_uart.readable, &resp, sizeof(resp));
}

static HRESULT controlbd_req_polling(const struct controlbd_req_any *req)
{
    struct controlbd_req_polling req_struct;
    memset(&req_struct, 0, sizeof(req_struct));
    memcpy_s(&req_struct, sizeof(req_struct), req, sizeof(req_struct));
    struct controlbd_resp_polling resp;
    memset(&resp, 0, sizeof(resp));
    controlbd_set_header(&resp.hdr, CONTROLBD_CMD_POLLING, 16);
    // TODO: Figure out output (pen vibration, etc)

    resp.ack = 1;
    resp.unk7 = 3;
    resp.unk8 = 1;
    resp.unk9 = 1;

    resp.btns_pressed = 0; // bit 1 is pen button, bit 2 is dodge
    resp.coord_x = 0x0;
    resp.coord_y = 0x0;
    resp.checksum = calc_checksum(&resp, sizeof(resp));

    return iobuf_write(&controlbd_uart.readable, &resp, sizeof(resp));
}

static HRESULT controlbd_req_ack_any(uint8_t cmd)
{
    struct controlbd_resp_any_ack resp;
    memset(&resp, 0, sizeof(resp));
    controlbd_set_header(&resp.hdr, cmd, 3);

    resp.ack = 1;
    resp.checksum = calc_checksum(&resp, sizeof(resp));

    return iobuf_write(&controlbd_uart.readable, &resp, sizeof(resp));
}
