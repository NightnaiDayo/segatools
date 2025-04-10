/*
    SEGA 837-15070-0X LED Controller Board Emulator

    Credits:
    837-15070-04 LED Controller Board Emulator (emihiok)
    837-15093-06 LED Controller Board Emulator (somewhatlurker, skogaby)
    (a/o June 2023)
*/

#include <windows.h>

#include <assert.h>
#include <process.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "board/led15070-cmd.h"
#include "board/led15070-frame.h"

#include "board/led15070.h"

#include "hook/iobuf.h"
#include "hook/iohook.h"

#include "hooklib/uart.h"

#include "util/dprintf.h"
#include "util/dump.h"

static HRESULT led15070_handle_irp(struct irp *irp);
static HRESULT led15070_handle_irp_locked(int board, struct irp *irp);

static HRESULT led15070_req_dispatch(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_reset(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_set_input(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_set_normal_12bit(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_set_normal_8bit(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_set_multi_flash_8bit(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_set_multi_fade_8bit(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_set_palette_7_normal_led(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_set_palette_6_flash_led(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_set_15dc_out(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_set_15gs_out(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_set_psc_max(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_set_fet_output(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_set_gs_palette(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_dc_update(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_gs_update(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_rotate(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_set_dc_data(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_eeprom_write(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_eeprom_read(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_ack_on(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_ack_off(int board, const struct led15070_req_any *req);
static HRESULT led15070_req_board_info(int board);
static HRESULT led15070_req_board_status(int board);
static HRESULT led15070_req_fw_sum(int board);
static HRESULT led15070_req_protocol_ver(int board);
static HRESULT led15070_req_to_boot_mode(int board);
static HRESULT led15070_req_fw_update(int board, const struct led15070_req_any *req);

static HRESULT led15070_eeprom_open(int board, wchar_t *path, HANDLE *handle);
static HRESULT led15070_eeprom_close(int board, wchar_t *path, HANDLE *handle);

static char led15070_board_num[8];
static uint8_t led15070_fw_ver;
static uint16_t led15070_fw_sum;
static uint8_t led15070_host_adr = 0x01;

#define led15070_nboards 2

typedef struct {
    CRITICAL_SECTION lock;
    bool started;
    HRESULT start_hr;
    struct uart boarduart;
    uint8_t written_bytes[520];
    uint8_t readable_bytes[520];
    uint8_t gs[32][4];
    uint8_t dc[32][3];
    uint8_t fet[3];
    uint8_t gs_palette[8][3];
    wchar_t eeprom_path[MAX_PATH];
    HANDLE eeprom_handle;
    uint8_t boardadr;
    bool enable_bootloader;
    bool enable_response;
} _led15070_per_board_vars;

_led15070_per_board_vars led15070_per_board_vars[led15070_nboards];

static io_led_init_t led_init;
static io_led_set_fet_output_t led_set_fet_output;
static io_led_dc_update_t led_dc_update;
static io_led_gs_update_t led_gs_update;

HRESULT led15070_hook_init(
        const struct led15070_config *cfg,
        io_led_init_t _led_init,
        io_led_set_fet_output_t _led_set_fet_output,
        io_led_dc_update_t _led_dc_update,
        io_led_gs_update_t _led_gs_update,
        unsigned int port_no[2])
{
    unsigned int num_boards = 0;

    assert(cfg != NULL);
    assert(_led_init != NULL);

    if (!cfg->enable) {
        return S_FALSE;
    }

    for (int i = 0; i < led15070_nboards; i++)
    {  
        if (cfg->port_no[i] != 0) {
            port_no[i] = cfg->port_no[i];
        }

        if (port_no[i] != 0) {
            num_boards++;
        }
    }

    assert(num_boards != 0);

    led_init = _led_init;
    led_set_fet_output = _led_set_fet_output;
    led_dc_update = _led_dc_update;
    led_gs_update = _led_gs_update;

    memcpy(led15070_board_num, cfg->board_number, sizeof(led15070_board_num));
    led15070_fw_ver = cfg->fw_ver;
    led15070_fw_sum = cfg->fw_sum;

    for (int i = 0; i < num_boards; i++)
    {
        _led15070_per_board_vars *v = &led15070_per_board_vars[i];

        InitializeCriticalSection(&v->lock);

        uart_init(&v->boarduart, port_no[i]);
        v->boarduart.baud.BaudRate = 115200;
        v->boarduart.written.bytes = v->written_bytes;
        v->boarduart.written.nbytes = sizeof(v->written_bytes);
        v->boarduart.readable.bytes = v->readable_bytes;
        v->boarduart.readable.nbytes = sizeof(v->readable_bytes);

        memset(v->gs, 0, sizeof(v->gs));
        memset(v->dc, 0, sizeof(v->dc));
        memset(v->fet, 0, sizeof(v->fet));
        memset(v->gs_palette, 0, sizeof(v->gs_palette));
        memset(v->eeprom_path, 0, sizeof(v->eeprom_path));
        v->eeprom_handle = NULL;

        swprintf_s
        (
            v->eeprom_path, MAX_PATH,
            L"%s\\led15070_eeprom_%d.bin",
            cfg->eeprom_path, i
        );

        /* Generate board EEPROM file if it doesn't already exist */
        led15070_eeprom_open(i, v->eeprom_path, &(v->eeprom_handle));
        led15070_eeprom_close(i, v->eeprom_path, &(v->eeprom_handle));

        v->boardadr = 0x11;
        v->enable_bootloader = false;
        v->enable_response = false;
    }

    dprintf("LED 15070: hook enabled.\n");

    return iohook_push_handler(led15070_handle_irp);
}

static HRESULT led15070_handle_irp(struct irp *irp)
{
    HRESULT hr;

    assert(irp != NULL);

    for (int i = 0; i < led15070_nboards; i++)
    {
        _led15070_per_board_vars *v = &led15070_per_board_vars[i];
        struct uart *boarduart = &v->boarduart;

        if (uart_match_irp(boarduart, irp))
        {
            CRITICAL_SECTION lock = v->lock;

            EnterCriticalSection(&lock);
            hr = led15070_handle_irp_locked(i, irp);
            LeaveCriticalSection(&lock);

            return hr;
        }
    }

    return iohook_invoke_next(irp);
}

static HRESULT led15070_handle_irp_locked(int board, struct irp *irp)
{
    struct led15070_req_any req;
    struct iobuf req_iobuf;
    HRESULT hr;

    _led15070_per_board_vars *v = &led15070_per_board_vars[board];
    struct uart *boarduart = &led15070_per_board_vars[board].boarduart;

    if (irp->op == IRP_OP_OPEN) {
        // Unfortunately the LED board UART gets opened and closed 
        // repeatedly

        if (!v->started) {
            dprintf("LED 15070: Starting LED backend\n");
            hr = led_init();

            v->started = true;
            v->start_hr = hr;

            if (FAILED(hr)) {
                dprintf("LED 15070: Backend error, LED controller "
                        "disconnected: %x\n",
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
#if defined(LOG_LED15070)
        dprintf("TX Buffer:\n");
        dump_iobuf(&boarduart->written);
#endif

        req_iobuf.bytes = (uint8_t*)&req;
        req_iobuf.nbytes = sizeof(req.hdr) + sizeof(req.cmd) + sizeof(req.payload);
        req_iobuf.pos = 0;

        hr = led15070_frame_decode(&req_iobuf, &boarduart->written);

        if (hr != S_OK) {
            if (FAILED(hr)) {
                dprintf("LED 15070: Deframe error: %x\n", (int) hr);
            }

            return hr;
        }

#if defined(LOG_LED15070)
        dprintf("Deframe Buffer:\n");
        dump_iobuf(&req_iobuf);
#endif

        hr = led15070_req_dispatch(board, &req);

        if (FAILED(hr)) {
            dprintf("LED 15070: Processing error: %x\n", (int) hr);
        }
    }
}

static HRESULT led15070_req_dispatch(int board, const struct led15070_req_any *req)
{
    switch (req->cmd) {
    case LED_15070_CMD_RESET:
        return led15070_req_reset(board, req);

    case LED_15070_CMD_SET_INPUT:
        return led15070_req_set_input(board, req);

    case LED_15070_CMD_SET_NORMAL_12BIT:
        return led15070_req_set_normal_12bit(board, req);

    case LED_15070_CMD_SET_NORMAL_8BIT:
        return led15070_req_set_normal_8bit(board, req);

    case LED_15070_CMD_SET_MULTI_FLASH_8BIT:
        return led15070_req_set_multi_flash_8bit(board, req);

    case LED_15070_CMD_SET_MULTI_FADE_8BIT:
        return led15070_req_set_multi_fade_8bit(board, req);

    case LED_15070_CMD_SET_PALETTE_7_NORMAL_LED:
        return led15070_req_set_palette_7_normal_led(board, req);

    case LED_15070_CMD_SET_PALETTE_6_FLASH_LED:
        return led15070_req_set_palette_6_flash_led(board, req);

    case LED_15070_CMD_SET_15DC_OUT:
        return led15070_req_set_15dc_out(board, req);

    case LED_15070_CMD_SET_15GS_OUT:
        return led15070_req_set_15gs_out(board, req);

    case LED_15070_CMD_SET_PSC_MAX:
        return led15070_req_set_psc_max(board, req);

    case LED_15070_CMD_SET_FET_OUTPUT:
        return led15070_req_set_fet_output(board, req);

    case LED_15070_CMD_SET_GS_PALETTE:
        return led15070_req_set_gs_palette(board, req);

    case LED_15070_CMD_DC_UPDATE:
        return led15070_req_dc_update(board, req);

    case LED_15070_CMD_GS_UPDATE:
        return led15070_req_gs_update(board, req);

    case LED_15070_CMD_ROTATE:
        return led15070_req_rotate(board, req);

    case LED_15070_CMD_SET_DC_DATA:
        return led15070_req_set_dc_data(board, req);

    case LED_15070_CMD_EEPROM_WRITE:
        return led15070_req_eeprom_write(board, req);

    case LED_15070_CMD_EEPROM_READ:
        return led15070_req_eeprom_read(board, req);

    case LED_15070_CMD_ACK_ON:
        return led15070_req_ack_on(board, req);

    case LED_15070_CMD_ACK_OFF:
        return led15070_req_ack_off(board, req);

    case LED_15070_CMD_BOARD_INFO:
        return led15070_req_board_info(board);

    case LED_15070_CMD_BOARD_STATUS:
        return led15070_req_board_status(board);

    case LED_15070_CMD_FW_SUM:
        return led15070_req_fw_sum(board);

    case LED_15070_CMD_PROTOCOL_VER:
        return led15070_req_protocol_ver(board);

    case LED_15070_CMD_TO_BOOT_MODE:
        return led15070_req_to_boot_mode(board);

    case LED_15070_CMD_FW_UPDATE:
        return led15070_req_fw_update(board, req);

    default:
        dprintf("LED 15070: Unhandled command %02x\n", req->cmd);

        return S_OK;
    }
}

static HRESULT led15070_req_reset(int board, const struct led15070_req_any *req)
{
    dprintf("LED 15070: Reset (board %u)\n", board);

    led15070_per_board_vars[board].enable_bootloader = false;
    led15070_per_board_vars[board].enable_response = true;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_RESET;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_set_input(int board, const struct led15070_req_any *req)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: Set input (board %u)\n", board);
#endif

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_SET_INPUT;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_set_normal_12bit(int board, const struct led15070_req_any *req)
{
    uint8_t idx = req->payload[0];
#if defined(LOG_LED15070)
    dprintf("LED 15070: Set LED - Normal 12bit (board %u, index %u)\n",
           board, idx);
#endif

    // TODO: Data for this command. Seen with Carol

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_SET_NORMAL_12BIT;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_set_normal_8bit(int board, const struct led15070_req_any *req)
{
    uint8_t idx = req->payload[0];
#if defined(LOG_LED15070)
    dprintf("LED 15070: Set LED - Normal 8bit (board %u, index %u)\n",
           board, idx);
#endif

    led15070_per_board_vars[board].gs[idx][0] = req->payload[1]; // R
    led15070_per_board_vars[board].gs[idx][1] = req->payload[2]; // G
    led15070_per_board_vars[board].gs[idx][2] = req->payload[3]; // B

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_SET_NORMAL_8BIT;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_set_multi_flash_8bit(int board, const struct led15070_req_any *req)
{
    uint8_t idx_start = req->payload[0];
    uint8_t idx_end = req->payload[1];
    uint8_t idx_skip = req->payload[2];

    // TODO: useful?
#if defined(LOG_LED15070)
    dprintf("LED 15070: Set LED - Multi flash 8bit (board %u, start %u, end %u, skip %u)\n",
           board, idx_start, idx_end, idx_skip);
#endif

    if (idx_skip > 0 && idx_skip <= (idx_end - idx_start + 1)) {
        idx_start += idx_skip;
    }

    int i = idx_start;
    do {
        led15070_per_board_vars[board].gs[i][0] = req->payload[3]; // R
        led15070_per_board_vars[board].gs[i][1] = req->payload[4]; // G
        led15070_per_board_vars[board].gs[i][2] = req->payload[5]; // B
        /* Always 0, tells the controller to immediately change to this color */
        led15070_per_board_vars[board].gs[i][3] = req->payload[6]; // Speed
        i++;
    } while (i < idx_end);

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_SET_MULTI_FLASH_8BIT;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_set_multi_fade_8bit(int board, const struct led15070_req_any *req)
{
    uint8_t idx_start = req->payload[0];
    uint8_t idx_end = req->payload[1];
    uint8_t idx_skip = req->payload[2];
#if defined(LOG_LED15070)
    dprintf("LED 15070: Set LED - Multi fade 8bit (board %u, start %u, end %u, skip %u)\n",
           board, idx_start, idx_end, idx_skip);
#endif

    if (idx_skip > 0 && idx_skip <= (idx_end - idx_start + 1)) {
        idx_start += idx_skip;
    }

    int i = idx_start;
    do {
        led15070_per_board_vars[board].gs[i][0] = req->payload[3]; // R
        led15070_per_board_vars[board].gs[i][1] = req->payload[4]; // G
        led15070_per_board_vars[board].gs[i][2] = req->payload[5]; // B
        led15070_per_board_vars[board].gs[i][3] = req->payload[6]; // Speed
        i++;
    } while (i < idx_end);

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_SET_MULTI_FADE_8BIT;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_set_palette_7_normal_led(int board, const struct led15070_req_any *req)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: Set palette - 7 Normal LED (board %u)\n", board);
#endif

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_SET_PALETTE_7_NORMAL_LED;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_set_palette_6_flash_led(int board, const struct led15070_req_any *req)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: Set palette - 6 Flash LED (board %u)\n", board);
#endif

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_SET_PALETTE_6_FLASH_LED;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_set_15dc_out(int board, const struct led15070_req_any *req)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: Set 15DC out (board %u)\n", board);
#endif

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_SET_15DC_OUT;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_set_15gs_out(int board, const struct led15070_req_any *req)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: Set 15GS out (board %u)\n", board);
#endif

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_SET_15GS_OUT;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_set_psc_max(int board, const struct led15070_req_any *req)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: Set PSC max (board %u)\n", board);
#endif

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_SET_PSC_MAX;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_set_fet_output(int board, const struct led15070_req_any *req)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: Set FET output (board %u)\n", board);
#endif

    led15070_per_board_vars[board].fet[0] = req->payload[0]; // R or FET0 intensity
    led15070_per_board_vars[board].fet[1] = req->payload[1]; // G or FET1 intensity
    led15070_per_board_vars[board].fet[2] = req->payload[2]; // B or FET2 intensity

    if (led_set_fet_output)
        led_set_fet_output(board, (const uint8_t*)led15070_per_board_vars[board].fet);

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_SET_FET_OUTPUT;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_set_gs_palette(int board, const struct led15070_req_any *req)
{
    uint8_t idx = req->payload[0];
#if defined(LOG_LED15070)
    dprintf("LED 15070: Set GS palette (board %u, index %u)\n", board, idx);
#endif

    led15070_per_board_vars[board].gs_palette[idx][0] = req->payload[1]; // R
    led15070_per_board_vars[board].gs_palette[idx][1] = req->payload[2]; // G
    led15070_per_board_vars[board].gs_palette[idx][2] = req->payload[3]; // B

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_SET_GS_PALETTE;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_dc_update(int board, const struct led15070_req_any *req)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: DC update (board %u)\n", board);
#endif

    if (led_dc_update)
        led_dc_update(board, (const uint8_t*)led15070_per_board_vars[board].dc);

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_DC_UPDATE;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_gs_update(int board, const struct led15070_req_any *req)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: GS update (board %u)\n", board);
#endif

    if (led_gs_update)
        led_gs_update(board, (const uint8_t*)led15070_per_board_vars[board].gs);

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_GS_UPDATE;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_rotate(int board, const struct led15070_req_any *req)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: Rotate (board %u)\n", board);
#endif

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_ROTATE;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_set_dc_data(int board, const struct led15070_req_any *req)
{
    uint8_t idx_start = req->payload[0];
    uint8_t idx_end = req->payload[1];
    uint8_t idx_skip = req->payload[2];
#if defined(LOG_LED15070)
    dprintf("LED 15070: Set DC data (board %u, start %u, end %u, skip %u)\n",
           board, idx_start, idx_end, idx_skip);
#endif

    if (idx_skip > 0 && idx_skip <= (idx_end - idx_start + 1)) {
        idx_start += idx_skip;
    }

    int i = idx_start;
    do {
        led15070_per_board_vars[board].dc[i][0] = req->payload[3]; // R
        led15070_per_board_vars[board].dc[i][1] = req->payload[4]; // G
        led15070_per_board_vars[board].dc[i][2] = req->payload[5]; // B
        i++;
    } while (i < idx_end);

    if (!led15070_per_board_vars[board].enable_response)
        return S_OK;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_SET_DC_DATA;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_eeprom_write(int board, const struct led15070_req_any *req)
{
    DWORD eeprom_bytes_written = 0;
    HRESULT hr;
    BOOL ok;

    uint8_t addr = req->payload[0];
    uint8_t data = req->payload[1];
#if defined(LOG_LED15070)
    dprintf("LED 15070: EEPROM write (board %u, address %02x, data %02x)\n",
            board, addr, data);
#endif

    if (addr > 0x07) {
        dprintf("LED 15070: Error -- Invalid EEPROM write address %02x\n",
                addr);

        return E_INVALIDARG;
    }

    hr = led15070_eeprom_open(
            board,
            led15070_per_board_vars[board].eeprom_path,
            &(led15070_per_board_vars[board].eeprom_handle));

    if (FAILED(hr)) {
        return hr;
    }

    hr = SetFilePointer(
            led15070_per_board_vars[board].eeprom_handle,
            addr,
            NULL,
            FILE_BEGIN);

    if (hr == INVALID_SET_FILE_POINTER) {
        dprintf("LED 10570: Error -- Failed to set pointer to EEPROM file "
                "(board %u)\n", board);
        led15070_eeprom_close(
                board,
                led15070_per_board_vars[board].eeprom_path,
                &(led15070_per_board_vars[board].eeprom_handle));

        return hr;
    }

    ok = WriteFile(
            led15070_per_board_vars[board].eeprom_handle,
            &data,
            sizeof(data),
            &eeprom_bytes_written, NULL);

    if (!ok || eeprom_bytes_written == 0) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        dprintf("LED 15070: Error -- Failed to write to EEPROM file: %x "
                "(board %u)\n", (int) hr, board);
        led15070_eeprom_close(
                board,
                led15070_per_board_vars[board].eeprom_path,
                &(led15070_per_board_vars[board].eeprom_handle));

        return hr;
    }

    hr = led15070_eeprom_close(
            board,
            led15070_per_board_vars[board].eeprom_path,
            &(led15070_per_board_vars[board].eeprom_handle));

    if (FAILED(hr)) {
        return hr;
    }

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 2 + 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_EEPROM_WRITE;
    resp.report = LED_15070_REPORT_OK;

    resp.data[0] = addr;
    resp.data[1] = data;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_eeprom_read(int board, const struct led15070_req_any *req)
{
    DWORD eeprom_bytes_read = 0;
    HRESULT hr;
    BOOL ok;

    uint8_t addr = req->payload[0];
    uint8_t data = 0;
#if defined(LOG_LED15070)
    dprintf("LED 15070: EEPROM read (board %u, address %02x)\n", board, addr);
#endif

    if (addr > 0x07) {
        dprintf("LED 15070: Error -- Invalid EEPROM read address %02x\n",
                addr);

        return E_INVALIDARG;
    }

    hr = led15070_eeprom_open(
            board,
            led15070_per_board_vars[board].eeprom_path,
            &(led15070_per_board_vars[board].eeprom_handle));

    if (FAILED(hr)) {
        return hr;
    }

    hr = SetFilePointer(
            led15070_per_board_vars[board].eeprom_handle,
            addr,
            NULL,
            FILE_BEGIN);

    if (hr == INVALID_SET_FILE_POINTER) {
        dprintf("LED 10570: Error -- Failed to set pointer to EEPROM file "
                "(board %u)\n", board);
        led15070_eeprom_close(
                board,
                led15070_per_board_vars[board].eeprom_path,
                &(led15070_per_board_vars[board].eeprom_handle));

        return hr;
    }

    ok = ReadFile(
            led15070_per_board_vars[board].eeprom_handle,
            &data,
            sizeof(data),
            &eeprom_bytes_read,
            NULL);

    if (!ok || eeprom_bytes_read == 0) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        dprintf("LED 15070: Error -- Failed to read from EEPROM file: %x "
                "(board %u)\n", (int) hr, board);
        led15070_eeprom_close(
                board,
                led15070_per_board_vars[board].eeprom_path,
                &(led15070_per_board_vars[board].eeprom_handle));

        return hr;
    }

    hr = led15070_eeprom_close(
            board,
            led15070_per_board_vars[board].eeprom_path,
            &(led15070_per_board_vars[board].eeprom_handle));

    if (FAILED(hr)) {
        return hr;
    }

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes =  1 + 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_EEPROM_READ;
    resp.report = LED_15070_REPORT_OK;

    resp.data[0] = data;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_ack_on(int board, const struct led15070_req_any *req)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: Acknowledge commands ON (board %u)\n", board);
#endif

    led15070_per_board_vars[board].enable_response = true;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_ACK_ON;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_ack_off(int board, const struct led15070_req_any *req)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: Acknowledge commands OFF (board %u)\n", board);
#endif

    led15070_per_board_vars[board].enable_response = false;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_ACK_OFF;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_board_info(int board)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: Get board info (board %u)\n", board);
#endif

    struct led15070_resp_board_info resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 10 + 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_BOARD_INFO;
    resp.report = LED_15070_REPORT_OK;

    memcpy(resp.board_num, led15070_board_num, sizeof(resp.board_num));
    resp.endcode = 0xff;
    resp.fw_ver = led15070_fw_ver;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_board_status(int board)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: Get board status (board %u)\n", board);
#endif

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 4 + 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_BOARD_STATUS;
    resp.report = LED_15070_REPORT_OK;

    resp.data[0] = 0; // Timeout status
    resp.data[1] = 0; // Timeout sec
    resp.data[2] = 0; // PWM IO
    resp.data[3] = 0; // FET timeout

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_fw_sum(int board)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: Get firmware checksum (board %u)\n", board);
#endif

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 2 + 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_FW_SUM;
    resp.report = LED_15070_REPORT_OK;

    resp.data[0] = (led15070_fw_sum >> 8) & 0xff;
    resp.data[1] = led15070_fw_sum & 0xff;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_protocol_ver(int board)
{
#if defined(LOG_LED15070)
    dprintf("LED 15070: Get protocol version (board %u)\n", board);
#endif

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3 + 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_PROTOCOL_VER;
    resp.report = LED_15070_REPORT_OK;

    if (led15070_per_board_vars[board].enable_bootloader) {
        resp.data[0] = 0; // BOOT mode
        resp.data[1] = 1; // Version major
        resp.data[2] = 1; // Version minor
    } else {
        resp.data[0] = 1; // APPLI mode
        resp.data[1] = 1; // Version major
        resp.data[2] = 4; // Version minor
    }

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_to_boot_mode(int board)
{
    dprintf("LED 15070: To boot mode (board %u)\n", board);

    led15070_per_board_vars[board].enable_bootloader = true;

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_TO_BOOT_MODE;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_req_fw_update(int board, const struct led15070_req_any *req)
{
    dprintf("LED 15070: Firmware update (UNIMPLEMENTED) (board %u)\n", board);

    struct led15070_resp_any resp;

    memset(&resp, 0, sizeof(resp));
    resp.hdr.sync = LED_15070_FRAME_SYNC;
    resp.hdr.dest_adr = led15070_host_adr;
    resp.hdr.src_adr = led15070_per_board_vars[board].boardadr;
    resp.hdr.nbytes = 3;

    resp.status = LED_15070_STATUS_OK;
    resp.cmd = LED_15070_CMD_FW_UPDATE;
    resp.report = LED_15070_REPORT_OK;

    return led15070_frame_encode(&led15070_per_board_vars[board].boarduart.readable, &resp, sizeof(resp.hdr) + resp.hdr.nbytes);
}

static HRESULT led15070_eeprom_open(int board, wchar_t *path, HANDLE *handle)
{
    DWORD eeprom_bytes_written;
    BYTE null_bytes[8] = { 0x00 };
    HRESULT hr;
    BOOL ok;

#if defined(LOG_LED15070)
    dprintf("LED 15070: Opening EEPROM file '%S' handle (board %u)\n", path, board);
#endif

    *handle = CreateFileW(
            path,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

    if (*handle == INVALID_HANDLE_VALUE) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        dprintf("LED 15070: Failed to open EEPROM file '%S' handle: %x "
                "(board %u)\n", path, (int) hr, board);

        return hr;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) return S_OK;

    ok = WriteFile(
            *handle,
            null_bytes,
            sizeof(null_bytes),
            &eeprom_bytes_written,
            NULL);

    if (!ok || eeprom_bytes_written != sizeof(null_bytes)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        dprintf("LED 15070: Failed to initialize empty EEPROM file '%S': %x "
                "(board %u)\n", path, (int) hr, board);

        return hr;
    }

    return S_OK;
}

static HRESULT led15070_eeprom_close(int board, wchar_t *path, HANDLE *handle)
{
    HRESULT hr;
    BOOL ok;

#if defined(LOG_LED15070)
    dprintf("LED 15070: Closing EEPROM file '%S' handle (board %u)\n", path, board);
#endif

    ok = CloseHandle(*handle);

    if (!ok) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        dprintf("LED 15070: Failed to close EEPROM file '%S' handle: %x "
                "(board %u)\n", path, (int) hr, board);

        return hr;
    }

    return S_OK;
}
