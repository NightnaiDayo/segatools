/*
    SEGA 837-15093-XX LED Controller Board emulator

    Supported variants:

    837-15093
    837-15093-01
    837-15093-04
    837-15093-05
    837-15093-06

    Credits:
    837-15093-XX LED Controller Board emulator (emihiok)
    837-15093-06 LED Controller Board emulator (somewhatlurker, skogaby)
*/

#include <windows.h>

#include <assert.h>
#include <process.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "board/led15093-cmd.h"
#include "board/led15093-frame.h"
#include "board/led15093.h"

#include "hook/iobuf.h"
#include "hook/iohook.h"

#include "hooklib/uart.h"

#include "util/dprintf.h"
#include "util/str.h"
#include "util/dump.h"

#define led15093_nboards 2
#define led15093_nnodes 1

#define led15093_led_count_max 66

typedef struct {
    uint8_t boardadr;
    uint8_t boardstatus[4];
    uint8_t fade_depth;
    uint8_t fade_cycle;
    uint8_t led[led15093_led_count_max][3];
    uint8_t led_bright[led15093_led_count_max][3];
    uint8_t led_count;
    uint8_t status_code;
    uint8_t report_code;
    bool enable_bootloader;
    bool enable_response;
} _led15093_per_node_vars;

typedef struct {
    CRITICAL_SECTION lock;
    bool started;
    HRESULT start_hr;
    struct uart boarduart;
    uint8_t written_bytes[520];
    uint8_t readable_bytes[520];
    _led15093_per_node_vars per_node_vars[led15093_nnodes];
} _led15093_per_board_vars;

_led15093_per_board_vars led15093_per_board_vars[led15093_nboards];

static HRESULT led15093_handle_irp(struct irp *irp);
static HRESULT led15093_handle_irp_locked(int board, struct irp *irp);

static HRESULT led15093_req_dispatch(int board, const union led15093_req_any *req);
static HRESULT led15093_req_reset(int board, const struct led15093_req_reset *req);
static HRESULT led15093_req_set_timeout(int board, const struct led15093_req_set_timeout *req);
static HRESULT led15093_req_set_response(int board, const struct led15093_req_set_disable_response *req);
static HRESULT led15093_req_set_id(int board, const struct led15093_req_set_id *req);
static HRESULT led15093_req_clear_id(int board, const union led15093_req_any *req);
static HRESULT led15093_req_set_max_bright(int board, const struct led15093_req_set_led *req);
static HRESULT led15093_req_update_led(int board, const union led15093_req_any *req);
static HRESULT led15093_req_set_led(int board, const struct led15093_req_set_led *req);
static HRESULT led15093_req_set_imm_led(int board, const struct led15093_req_set_led *req);
static HRESULT led15093_req_set_fade_led(int board, const struct led15093_req_set_led *req);
static HRESULT led15093_req_set_fade_level(int board, const struct led15093_req_set_fade_level *req);
static HRESULT led15093_req_set_fade_shift(int board, const struct led15093_req_set_fade_shift *req);
static HRESULT led15093_req_set_auto_shift(int board, const struct led15093_req_set_auto_shift *req);
static HRESULT led15093_req_get_board_info(int board, const union led15093_req_any *req);
static HRESULT led15093_req_get_board_status(int board, const struct led15093_req_get_board_status *req);
static HRESULT led15093_req_get_fw_sum(int board, const union led15093_req_any *req);
static HRESULT led15093_req_get_protocol_ver(int board, const union led15093_req_any *req);
static HRESULT led15093_req_set_bootmode(int board, const union led15093_req_any *req);
static HRESULT led15093_req_fw_update(int board, const union led15093_req_any *req);

static void led15093_set_status(int board, int id, uint8_t type, uint8_t status);
static void led15093_set_report(int board, int id, uint8_t report);
static void led15093_clear_status(int board, int id);
static void led15093_clear_report(int board, int id);

static char led15093_board_num[8];
static char led15093_chip_num[5];
static char led15093_boot_chip_num[5];
static uint8_t led15093_fw_ver;
static uint16_t led15093_fw_sum;
static uint8_t led15093_board_adr = 1;
static uint8_t led15093_host_adr = 1;

static io_led_init_t led_init;
static io_led_set_leds_t set_leds;

HRESULT led15093_hook_init(
    const struct led15093_config *cfg,
    io_led_init_t _led_init,
    io_led_set_leds_t _set_leds,
    unsigned int port_no[2])
{
    unsigned int num_boards = 0;

    assert(cfg != NULL);
    assert(_led_init != NULL);
    assert(_set_leds != NULL);

    if (!cfg->enable) {
        return S_FALSE;
    }

    for (int i = 0; i < led15093_nboards; i++)
    {  
        if (cfg->port_no[i] != 0) {
            port_no[i] = cfg->port_no[i];
        }

        if (port_no[i] != 0) {
            num_boards++;
        }
    }

    assert(num_boards != 0);

    led15093_board_adr = num_boards;
    led15093_host_adr = num_boards == 2 ? 1 : 2;

    led_init = _led_init;
    set_leds = _set_leds;

    memcpy(led15093_board_num, cfg->board_number, sizeof(led15093_board_num));
    memcpy(led15093_chip_num, cfg->chip_number, sizeof(led15093_chip_num));
    memcpy(led15093_boot_chip_num, cfg->boot_chip_number, sizeof(led15093_boot_chip_num));
    led15093_fw_ver = cfg->fw_ver;
    led15093_fw_sum = cfg->fw_sum;

    for (int i = 0; i < num_boards; i++)
    {
        _led15093_per_board_vars *vb = &led15093_per_board_vars[i];

        InitializeCriticalSection(&vb->lock);

        uart_init(&vb->boarduart, port_no[i]);
        if (cfg->high_baudrate) {
            vb->boarduart.baud.BaudRate = 460800;
        } else {
            vb->boarduart.baud.BaudRate = 115200;
        }
        vb->boarduart.written.bytes = vb->written_bytes;
        vb->boarduart.written.nbytes = sizeof(vb->written_bytes);
        vb->boarduart.readable.bytes = vb->readable_bytes;
        vb->boarduart.readable.nbytes = sizeof(vb->readable_bytes);

        for (int j = 0; j < led15093_nnodes; j++)
        {
            _led15093_per_node_vars *vn = &vb->per_node_vars[j];

            memset(vn->led, 0, sizeof(vn->led));
            memset(vn->led_bright, 0x3F, sizeof(vn->led_bright));
            vn->led_count = led15093_led_count_max;
            vn->fade_depth = 32;
            vn->fade_cycle = 8;
            led15093_clear_status(i, 1 + j);
            led15093_clear_report(i, 1 + j);
            vn->boardstatus[3] = 1; // DIPSW1 ON
            vn->boardadr = led15093_board_adr + j;
            vn->enable_bootloader = false;
            vn->enable_response = true;
        }
    }

    dprintf("LED 15093: hook enabled.\n");

    return iohook_push_handler(led15093_handle_irp);
}

static HRESULT led15093_handle_irp(struct irp *irp)
{
    HRESULT hr;

    assert(irp != NULL);

    for (int i = 0; i < led15093_nboards; i++)
    {
        _led15093_per_board_vars *v = &led15093_per_board_vars[i];
        struct uart *boarduart = &v->boarduart;

        if (uart_match_irp(boarduart, irp))
        {
            CRITICAL_SECTION lock = v->lock;

            EnterCriticalSection(&lock);
            hr = led15093_handle_irp_locked(i, irp);
            LeaveCriticalSection(&lock);

            return hr;
        }
    }

    return iohook_invoke_next(irp);
}

static HRESULT led15093_handle_irp_locked(int board, struct irp *irp)
{
    union led15093_req_any req;
    struct iobuf req_iobuf;
    HRESULT hr;

    _led15093_per_board_vars *v = &led15093_per_board_vars[board];
    struct uart *boarduart = &led15093_per_board_vars[board].boarduart;

    if (irp->op == IRP_OP_OPEN) {
        // Unfortunately the LED board UART gets opened and closed repeatedly

        if (!v->started) {
            dprintf("LED 15093: Starting LED backend\n");
            hr = led_init();

            // hr = S_OK;
            v->started = true;
            v->start_hr = hr;

            if (FAILED(hr)) {
                dprintf("LED 15093: Backend error, LED board disconnected: "
                        "%x\n",
                        (int) hr);

                return hr;
            }
        } else {
            hr = v->start_hr;

            if (FAILED(hr)) {
                return hr;
            }
        }
    }

    hr = uart_handle_irp(boarduart, irp);

    if (FAILED(hr) || irp->op != IRP_OP_WRITE) {
        return hr;
    }

    for (;;) {
#if defined(LOG_LED15093)
        dprintf("TX Buffer:\n");
        dump_iobuf(&boarduart->written);
#endif

        req_iobuf.bytes = (uint8_t*)&req;
        req_iobuf.nbytes = sizeof(req.hdr) + sizeof(req.payload);
        req_iobuf.pos = 0;

        hr = led15093_frame_decode(&req_iobuf, &boarduart->written);

        if (hr != S_OK) {
            if (hr == HRESULT_FROM_WIN32(ERROR_CRC)) {
                led15093_set_status(
                        board,
                        2,
                        LED_15093_STATUS_TYPE_UART,
                        LED_15093_STATUS_UART_ERR_SUM);
            }

            if (FAILED(hr)) {
                dprintf("LED 15093: Deframe error: %x\n", (int) hr);
            }

            return hr;
        }

#if defined(LOG_LED15093)
        dprintf("Deframe Buffer:\n");
        dump_iobuf(&req_iobuf);
#endif

        hr = led15093_req_dispatch(board, &req);

        if (FAILED(hr)) {
            dprintf("LED 15093: Processing error: %x\n", (int) hr);
        }

        /* TODO: We should wait for a get board status request with a clear flag,
           instead of doing this here. */
        led15093_clear_status(board, req.hdr.dest_adr);
        led15093_clear_report(board, req.hdr.dest_adr);
    }
}

static HRESULT led15093_req_dispatch(int board, const union led15093_req_any *req)
{
    switch (req->payload[4]) {
    case LED_15093_CMD_RESET:
        return led15093_req_reset(board, &req->reset);

    case LED_15093_CMD_SET_TIMEOUT:
        return led15093_req_set_timeout(board, &req->set_timeout);

    case LED_15093_CMD_SET_DISABLE_RESPONSE:
        return led15093_req_set_response(board, &req->set_disable_response);

    case LED_15093_CMD_SET_ID:
        return led15093_req_set_id(board, &req->set_id);

    case LED_15093_CMD_CLEAR_ID:
        return led15093_req_clear_id(board, req);

    case LED_15093_CMD_SET_MAX_BRIGHT:
        return led15093_req_set_max_bright(board, &req->set_led);

    case LED_15093_CMD_UPDATE_LED:
        return led15093_req_update_led(board, req);

    case LED_15093_CMD_SET_LED:
        return led15093_req_set_led(board, &req->set_led);

    case LED_15093_CMD_SET_IMM_LED:
    // case LED_15093_CMD_SET_IMM_LED_LEGACY:
        return led15093_req_set_imm_led(board, &req->set_led);

    case LED_15093_CMD_SET_FADE_LED:
        return led15093_req_set_fade_led(board, &req->set_led);

    case LED_15093_CMD_SET_FADE_LEVEL:
        return led15093_req_set_fade_level(board, &req->set_fade_level);

    case LED_15093_CMD_SET_FADE_SHIFT:
        return led15093_req_set_fade_shift(board, &req->set_fade_shift);

    case LED_15093_CMD_SET_AUTO_SHIFT:
        return led15093_req_set_auto_shift(board, &req->set_auto_shift);

    case LED_15093_CMD_GET_BOARD_INFO:
        return led15093_req_get_board_info(board, req);

    case LED_15093_CMD_GET_BOARD_STATUS:
        return led15093_req_get_board_status(board, &req->get_board_status);

    case LED_15093_CMD_GET_FW_SUM:
        return led15093_req_get_fw_sum(board, req);

    case LED_15093_CMD_GET_PROTOCOL_VER:
        return led15093_req_get_protocol_ver(board, req);

    case LED_15093_CMD_SET_BOOTMODE:
        return led15093_req_set_bootmode(board, req);

    case LED_15093_CMD_FW_UPDATE:
        return led15093_req_fw_update(board, req);

    default:
        dprintf("LED 15093: Unhandled command %02x\n", req->payload[4]);
        led15093_set_report(
                board,
                req->hdr.dest_adr,
                LED_15093_REPORT_ERR2);

        return S_OK;
    }
}

static HRESULT led15093_req_reset(int board, const struct led15093_req_reset *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    dprintf("LED 15093: Reset (board %d, node %d, type %02x)\n",
            board, req->hdr.dest_adr, req->r_type);

    if (req->r_type != 0xd9)
        dprintf("LED 15093: Warning -- Unknown reset type %02x\n", req->r_type);

    /* A dest_adr of 0 means that this message is addressed to all nodes */
    if (req->hdr.dest_adr == 0) {
        for (int i = 0; i < led15093_nnodes; i++) {
            led15093_per_board_vars[board].per_node_vars[i].enable_bootloader = false;
            led15093_per_board_vars[board].per_node_vars[i].enable_response = true;
        }
    } else {
        v->enable_bootloader = false;
        v->enable_response = true;
    }

    struct led15093_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_RESET;
    resp.report = v->report_code;

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_set_timeout(int board, const struct led15093_req_set_timeout *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    dprintf("LED 15093: Set timeout (board %u, count %u)\n", board, req->count);

    struct led15093_resp_timeout resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 2 + 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_SET_TIMEOUT;
    resp.report = v->report_code;

    resp.count_upper = (req->count >> 8) & 0xff;
    resp.count_lower = req->count & 0xff;

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_set_response(int board, const struct led15093_req_set_disable_response *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    if (req->hdr.nbytes > 1) {
        dprintf("LED 15093: Set LED response setting (board %d, node %d, SW %d)\n",
                board, req->hdr.dest_adr, req->sw);
        v->enable_response = !req->sw;
    } else {
        dprintf("LED 15093: Check LED response setting (board %d, node %d)\n",
                board, req->hdr.dest_adr);
    }

    struct led15093_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 1 + 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_SET_DISABLE_RESPONSE;
    resp.report = v->report_code;

    resp.data[0] = !v->enable_response;


    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_set_id(int board, const struct led15093_req_set_id *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    dprintf("LED 15093: Set ID (board %d, requested node ID %d)\n",
            board, req->id);

    if (req->id == 0 || req->id > 8) {
        dprintf("LED 15093: Error -- Invalid node ID requested: %d\n",
                req->id);
        led15093_set_status(
                board,
                v->boardadr,
                LED_15093_STATUS_TYPE_CMD,
                LED_15093_STATUS_CMD_ERR_PARAM);

        return HRESULT_FROM_WIN32(ERROR_BAD_UNIT);
    }

    /* A dest_adr of 0 means that this message is addressed to any unassigned
       node..? */
    if (req->hdr.dest_adr == 0) {
        bool slot_found = false;

        for (int i = 0; i < led15093_nnodes && slot_found == false; i++) {
            if (led15093_per_board_vars[board].per_node_vars[i].boardadr == 0) {
                led15093_per_board_vars[board].per_node_vars[i].boardadr = req->id;
                slot_found = true;
            }
        }

        if (!slot_found) {
            dprintf("LED 15093: Warning -- Unable to assign requested node "
                    "%d, all slots full\n", req->hdr.dest_adr);
        }
    } else {
        /* Overwrite current ID. Probably no use case for this but we do it
           anyway */
        v->boardadr = req->id;
    }
    struct led15093_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_SET_ID;
    resp.report = v->report_code;

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_clear_id(int board, const union led15093_req_any *req)
{
   uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    dprintf("LED 15093: Clear ID (board %d, node %d)\n",
            board, req->hdr.dest_adr);

    /* A dest_adr of 0 means that this message is addressed to all nodes */
    if (req->hdr.dest_adr == 0) {
        for (int i = 0; i < led15093_nnodes; i++) {
            led15093_per_board_vars[board].per_node_vars[i].boardadr = 0;
        }
    } else {
        /* Overwrite current ID. Probably no use case for this but we do it
           anyway */
        v->boardadr = 0;
    }

    struct led15093_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_CLEAR_ID;
    resp.report = v->report_code;

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_set_max_bright(int board, const struct led15093_req_set_led *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    dprintf("LED 15093: Set maximum LED brightness (board %d, node %d)\n",
            board, req->hdr.dest_adr);

    if ((req->hdr.nbytes - 1) > (v->led_count * 3)) {
        dprintf("LED 15093: Error -- Invalid LED count\n");
        led15093_set_report(board, req->hdr.dest_adr, LED_15093_REPORT_ERR1);

        return E_INVALIDARG;
    }

    /* 15093 equivalent of 15070's "set DC data". 63 (0x3F) is, again, the
       default. */

    memcpy(v->led_bright, req->data, req->hdr.nbytes - 1);

    // fgo_dll.led_gr_set_max_bright((const uint8_t*)&v->led_bright);

    if (!v->enable_response)
        return S_OK;

    struct led15093_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_SET_MAX_BRIGHT;
    resp.report = v->report_code;

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}


static HRESULT led15093_req_update_led(int board, const union led15093_req_any *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    // dprintf("LED 15093: Update LED (board %d, node %d)\n",
    //        board, req->hdr.dest_adr);

    // fgo_dll.led_gr_set_imm((const uint8_t*)&v->led);

    if (!v->enable_response)
        return S_OK;

    struct led15093_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_UPDATE_LED;
    resp.report = v->report_code;

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_set_led(int board, const struct led15093_req_set_led *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    // dprintf("LED 15093: Set LED (board %d, node %d)\n",
    //        board, req->hdr.dest_adr);

    if ((req->hdr.nbytes - 1) > (v->led_count * 3)) {
        dprintf("LED 15093: Error -- Invalid LED count\n");
        led15093_set_report(board, req->hdr.dest_adr, LED_15093_REPORT_ERR1);

        return E_INVALIDARG;
    }

    memcpy(v->led, req->data, req->hdr.nbytes - 1);

    if (!v->enable_response)
        return S_OK;

    struct led15093_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_SET_LED;
    resp.report = v->report_code;

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_set_imm_led(int board, const struct led15093_req_set_led *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    // dprintf("LED 15093: Set and immediately draw LED (board %d, node %d)\n",
    //        board, req->hdr.dest_adr);

    if ((req->hdr.nbytes - 1) > (v->led_count * 3)) {
        dprintf("LED 15093: Error -- Invalid LED count\n");
        led15093_set_report(board, req->hdr.dest_adr, LED_15093_REPORT_ERR1);

        return E_INVALIDARG;
    }

    // Return the current LED data, remove const qualifier
    set_leds(board, (uint8_t *) req->data);

    memcpy(v->led, req->data, req->hdr.nbytes - 1);
    // fgo_dll.led_gr_set_imm((const uint8_t*)&v->led);

    if (!v->enable_response)
        return S_OK;

    struct led15093_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 3;

    resp.status = v->status_code;
    if (req->cmd == LED_15093_CMD_SET_IMM_LED) {
        resp.cmd = LED_15093_CMD_SET_IMM_LED;
    }
    // else {
    //     resp.cmd = LED_15093_CMD_SET_IMM_LED_LEGACY;
    // }
    resp.report = v->report_code;

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_set_fade_led(int board, const struct led15093_req_set_led *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    // dprintf("LED 15093: Set and fade LED (board %d, node %d)\n",
    //        board, req->hdr.dest_adr);

    if ((req->hdr.nbytes - 1) > (v->led_count * 3)) {
        dprintf("LED 15093: Error -- Invalid LED count\n");
        led15093_set_report(board, req->hdr.dest_adr, LED_15093_REPORT_ERR1);

        return E_INVALIDARG;
    }

    memcpy(v->led, req->data, req->hdr.nbytes - 1);

    // fgo_dll.led_gr_set_fade(
    //         (const uint8_t*)v->led,
    //         v->fade_depth,
    //         v->fade_cycle);

    if (!v->enable_response)
        return S_OK;

    struct led15093_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_SET_FADE_LED;
    resp.report = v->report_code;

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_set_fade_level(int board, const struct led15093_req_set_fade_level *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    dprintf("LED 15093: Set fade level (board %d, node %d, depth %d, cycle %d)\n",
            board, req->hdr.dest_adr, req->depth, req->cycle);

    v->fade_depth = req->depth;
    v->fade_cycle = req->cycle;

    if (!v->enable_response)
        return S_OK;

    struct led15093_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_SET_FADE_LEVEL;
    resp.report = v->report_code;

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_set_fade_shift(int board, const struct led15093_req_set_fade_shift *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    // dprintf("LED 15093: Set fade shift (board %d, node %d)\n",
    //        board, req->hdr.dest_adr);

    if (!v->enable_response)
        return S_OK;

    struct led15093_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_SET_FADE_SHIFT;
    resp.report = v->report_code;

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_set_auto_shift(int board, const struct led15093_req_set_auto_shift *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    dprintf("LED 15093: Set auto shift (board %d, node %d)\n",
           board, req->hdr.dest_adr);

    /* There's actually a lot of conflicting info about this command... It
       seems they changed the arguments between the -04 and -05 board variants,
       and also changed the command ID from 0x87 to 0x86.

       Fortunately, only the -05 variant actually uses this in any shipping
       game, so we don't need to implement support for the legacy command
       version. */

    v->led_count = req->count;

    if (!v->enable_response)
        return S_OK;

    struct led15093_resp_set_auto_shift resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 1 + 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_SET_AUTO_SHIFT;
    resp.report = v->report_code;

    resp.count = v->led_count;

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_get_board_info(int board, const union led15093_req_any *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    dprintf("LED 15093: Get board info (board %d, node %d)\n",
            board, req->hdr.dest_adr);

    if (str_eq(led15093_board_num, "15093")) {
        struct led15093_resp_board_info_legacy resp;

        memset(&resp, 0, sizeof(resp));
        resp.hdr.sync = LED_15093_FRAME_SYNC;
        resp.hdr.dest_adr = led15093_host_adr;
        resp.hdr.src_adr = v->boardadr;
        resp.hdr.nbytes = 7 + 3;

        resp.status = v->status_code;
        resp.cmd = LED_15093_CMD_GET_BOARD_INFO;
        resp.report = v->report_code;

        memcpy(resp.board_num, led15093_board_num, sizeof(resp.board_num));
        resp.endcode = 0xff;
        resp.fw_ver = led15093_fw_ver;

        return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
    } else {
        struct led15093_resp_board_info resp;

        memset(&resp, 0, sizeof(resp));
        resp.hdr.sync = LED_15093_FRAME_SYNC;
        resp.hdr.dest_adr = led15093_host_adr;
        resp.hdr.src_adr = v->boardadr;
        resp.hdr.nbytes = 18 + 3;

        resp.status = v->status_code;
        resp.cmd = LED_15093_CMD_GET_BOARD_INFO;
        resp.report = v->report_code;

        memcpy(resp.board_num, led15093_board_num, sizeof(resp.board_num));
        resp.lf = 0x0a;
        if (v->enable_bootloader) {
            memcpy(resp.chip_num, led15093_boot_chip_num, sizeof(resp.chip_num));
        } else {
            memcpy(resp.chip_num, led15093_chip_num, sizeof(resp.chip_num));
        }
        resp.endcode = 0xff;
        resp.fw_ver = led15093_fw_ver;
        resp.rx_buf = 204; // Must be 204 regardless of active LED count

        return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
    }
}

static HRESULT led15093_req_get_board_status(int board, const struct led15093_req_get_board_status *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    dprintf("LED 15093: Get board status (board %d, node %d, clear %d)\n",
            board, req->hdr.dest_adr, req->clear);

    if (req->clear) {
        led15093_clear_status(board, req->hdr.dest_adr);
    }

    struct led15093_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = (v->enable_bootloader) ? (3 + 3) : (4 + 3);

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_GET_BOARD_STATUS;
    resp.report = v->report_code;

    if (v->enable_bootloader) {
        resp.data[0] = v->boardstatus[0]; // Board flags
        resp.data[1] = v->boardstatus[1]; // UART flags
        resp.data[2] = v->boardstatus[2]; // Command flags
    } else {
        resp.data[0] = v->boardstatus[0]; // Board flags
        resp.data[1] = v->boardstatus[1]; // UART flags
        resp.data[2] = v->boardstatus[2]; // Command flags
        resp.data[3] = v->boardstatus[3]; // DIPSW
    }

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_get_fw_sum(int board, const union led15093_req_any *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    dprintf("LED 15093: Get firmware checksum (board %d, node %d)\n",
            board, req->hdr.dest_adr);

    struct led15093_resp_fw_sum resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 2 + 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_GET_FW_SUM;
    resp.report = v->report_code;

    resp.sum_upper = (led15093_fw_sum >> 8) & 0xff;
    resp.sum_lower = led15093_fw_sum & 0xff;

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_get_protocol_ver(int board, const union led15093_req_any *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    dprintf("LED 15093: Get protocol version (board %d, node %d)\n",
            board, req->hdr.dest_adr);

    struct led15093_resp_protocol_ver resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 3 + 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_GET_PROTOCOL_VER;
    resp.report = v->report_code;

    if (v->enable_bootloader) {
        resp.mode = 0; // BOOT mode
        resp.major_ver = 1;
        resp.minor_ver = 1;
    } else {
        resp.mode = 1; // APPLI mode
        resp.major_ver = 1;
        resp.minor_ver = 4;
    }

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_set_bootmode(int board, const union led15093_req_any *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    dprintf("LED 15093: Set bootmode (board %d, node %d)\n",
            board, req->hdr.dest_adr);

    v->enable_bootloader = true;

    struct led15093_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 1 + 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_SET_BOOTMODE;
    resp.report = v->report_code;

    resp.data[0] = 1; // IDK but this seems to fix test mode?????

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15093_req_fw_update(int board, const union led15093_req_any *req)
{
    uint8_t id_idx = (req->hdr.dest_adr < 2) ? 0 : req->hdr.dest_adr - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    dprintf("LED 15093: Firmware update (UNIMPLEMENTED) (board %d, node %d)\n",
            board, req->hdr.dest_adr);

    struct led15093_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15093_FRAME_SYNC;
    resp.hdr.dest_adr = led15093_host_adr;
    resp.hdr.src_adr = v->boardadr;
    resp.hdr.nbytes = 3;

    resp.status = v->status_code;
    resp.cmd = LED_15093_CMD_FW_UPDATE;
    resp.report = v->report_code;

    return led15093_frame_encode(&led15093_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static void led15093_set_status(int board, int id, uint8_t type, uint8_t status)
{
    uint8_t id_idx = (id < 2) ? 0 : id - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];


    if (type == LED_15093_STATUS_TYPE_BOARD) {
        /* Set board flag */
        v->boardstatus[0] |= status;
    } else if (type == LED_15093_STATUS_TYPE_UART) {
        /* Set UART flag */
        v->boardstatus[1] |= status;
        /* Set response status code */
        switch (status) {
        case LED_15093_STATUS_UART_ERR_SUM:
            v->status_code = LED_15093_STATUS_ERR_SUM;
            break;
        case LED_15093_STATUS_UART_ERR_PARITY:
            v->status_code = LED_15093_STATUS_ERR_PARITY;
            break;
        case LED_15093_STATUS_UART_ERR_FRAMING:
            v->status_code = LED_15093_STATUS_ERR_FRAMING;
            break;
        case LED_15093_STATUS_ERR_OVERRUN:
            v->status_code = LED_15093_STATUS_ERR_OVERRUN;
            break;
        case LED_15093_STATUS_ERR_BUFFER_OVERFLOW:
            v->status_code = LED_15093_STATUS_ERR_BUFFER_OVERFLOW;
            break;
        default:
            break;
        }
    } else if (type == LED_15093_STATUS_TYPE_CMD) {
        /* Set command flag */
        v->boardstatus[2] |= status;
    }
}

static void led15093_set_report(int board, int id, uint8_t report)
{
    uint8_t id_idx = (id < 2) ? 0 : id - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    v->report_code = report;
}

static void led15093_clear_status(int board, int id)
{
    uint8_t id_idx = (id < 2) ? 0 : id - 2;
    _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

    if (id == 0)
    {
        for (int i = 0; i < led15093_nnodes; i++) {
            for (int j = 0; j < (_countof(v->boardstatus) - 1); j++) {
                led15093_per_board_vars[board].per_node_vars[i].boardstatus[j] = 0;
            }

            led15093_per_board_vars[board].per_node_vars[i].status_code = LED_15093_STATUS_OK;
        }
    }
    else
    {
        v->boardstatus[0] = 0; // Board flags
        v->boardstatus[1] = 0; // UART flags
        v->boardstatus[2] = 0; // Command flags

        v->status_code = LED_15093_STATUS_OK;
    }
}

static void led15093_clear_report(int board, int id)
{
    if (id == 0)
    {
        for (int i = 0; i < led15093_nnodes; i++) {
            led15093_per_board_vars[board].per_node_vars[i].report_code = LED_15093_REPORT_OK;
        }
    }
    else
    {
        uint8_t id_idx = (id < 2) ? 0 : id - 2;
        _led15093_per_node_vars *v = &led15093_per_board_vars[board].per_node_vars[id_idx];

        v->report_code = LED_15093_STATUS_OK;
    }
}
