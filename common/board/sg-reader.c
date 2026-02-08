#include <windows.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "board/aime-dll.h"
#include "board/sg-led.h"
#include "board/sg-nfc.h"
#include "board/sg-reader.h"

#include "hook/iohook.h"

#include "hooklib/uart.h"

#include "util/dprintf.h"
#include "util/dump.h"

static HRESULT sg_reader_handle_irp(struct irp *irp);
static HRESULT sg_reader_handle_irp_locked(struct irp *irp);
static HRESULT sg_reader_nfc_poll(void *ctx);
static HRESULT sg_reader_nfc_get_aime_id(
        void *ctx,
        uint8_t *luid,
        size_t luid_size);
static HRESULT sg_reader_nfc_get_felica_id(void *ctx, uint64_t *IDm);
static HRESULT sg_reader_nfc_get_mifare_uid(
        void *ctx,
        uint8_t *uid,
        size_t uid_size);
static HRESULT sg_reader_nfc_mifare_select(
        void *ctx,
        const uint8_t *uid,
        size_t uid_size);
static HRESULT sg_reader_nfc_mifare_set_key(
        void *ctx,
        uint8_t key_type,
        const uint8_t *key,
        size_t key_size);
static HRESULT sg_reader_nfc_mifare_authenticate(
        void *ctx,
        uint8_t key_type,
        const uint8_t *payload,
        size_t payload_size);
static HRESULT sg_reader_nfc_mifare_read_block(
        void *ctx,
        const uint8_t *uid,
        size_t uid_size,
        uint8_t block_no,
        uint8_t *block,
        size_t block_size);
static HRESULT sg_reader_nfc_felica_transact(
        void *ctx,
        const uint8_t *req,
        size_t req_size,
        uint8_t *res,
        size_t res_size,
        size_t *res_size_written);
static HRESULT sg_reader_nfc_radio_on(void *ctx);
static HRESULT sg_reader_nfc_radio_off(void *ctx);
static HRESULT sg_reader_nfc_to_update_mode(void *ctx);
static HRESULT sg_reader_nfc_send_hex_data(
        void *ctx,
        const uint8_t *payload,
        size_t payload_size,
        uint8_t *status_out);
static void sg_reader_led_set_color(void *ctx, uint8_t r, uint8_t g, uint8_t b);

static const struct sg_nfc_ops sg_reader_nfc_ops = {
    .poll           = sg_reader_nfc_poll,
    .get_aime_id    = sg_reader_nfc_get_aime_id,
    .get_felica_id  = sg_reader_nfc_get_felica_id,
    .get_mifare_uid = sg_reader_nfc_get_mifare_uid,
    .mifare_select  = sg_reader_nfc_mifare_select,
    .mifare_set_key = sg_reader_nfc_mifare_set_key,
    .mifare_authenticate = sg_reader_nfc_mifare_authenticate,
    .mifare_read_block = sg_reader_nfc_mifare_read_block,
    .felica_transact = sg_reader_nfc_felica_transact,
    .radio_on = sg_reader_nfc_radio_on,
    .radio_off = sg_reader_nfc_radio_off,
    .to_update_mode = sg_reader_nfc_to_update_mode,
    .send_hex_data = sg_reader_nfc_send_hex_data,
};

static const struct sg_led_ops sg_reader_led_ops = {
    .set_color          = sg_reader_led_set_color,
};

static CRITICAL_SECTION sg_reader_lock;
static bool sg_reader_started;
static HRESULT sg_reader_start_hr;
static struct uart sg_reader_uart;
static uint8_t sg_reader_written_bytes[520];
static uint8_t sg_reader_readable_bytes[520];
static struct sg_nfc sg_reader_nfc;
static struct sg_led sg_reader_led;

HRESULT sg_reader_hook_init(
        const struct aime_config *cfg,
        unsigned int default_port_no,
        unsigned int gen,
        HINSTANCE self)
{
    HRESULT hr;

    assert(cfg != NULL);
    assert(self != NULL);

    if (!cfg->enable) {
        return S_FALSE;
    }

    hr = aime_dll_init(&cfg->dll, self);

    if (FAILED(hr)) {
        return hr;
    }

    unsigned int port_no = cfg->port_no;
    if (port_no == 0){
        port_no = default_port_no;
    }

    if (cfg->gen != 0) {
        gen = cfg->gen;
    }

    if (gen < 1 || gen > 3) {
        dprintf("NFC Assembly: Invalid reader generation: %u\n", gen);

        return E_INVALIDARG;
    }

    sg_nfc_init(&sg_reader_nfc, 0x00, &sg_reader_nfc_ops, gen, cfg->proxy_flag, cfg->authdata_path, NULL);
    sg_led_init(&sg_reader_led, 0x08, &sg_reader_led_ops, gen, NULL);

    InitializeCriticalSection(&sg_reader_lock);

    if (!cfg->high_baudrate) {
        sg_reader_uart.baud.BaudRate = 38400;
    }

    dprintf("NFC Assembly: enabling (port=%d)\n", port_no);
    uart_init(&sg_reader_uart, port_no);
    sg_reader_uart.written.bytes = sg_reader_written_bytes;
    sg_reader_uart.written.nbytes = sizeof(sg_reader_written_bytes);
    sg_reader_uart.readable.bytes = sg_reader_readable_bytes;
    sg_reader_uart.readable.nbytes = sizeof(sg_reader_readable_bytes);

    return iohook_push_handler(sg_reader_handle_irp);
}

static HRESULT sg_reader_handle_irp(struct irp *irp)
{
    HRESULT hr;

    assert(irp != NULL);

    if (!uart_match_irp(&sg_reader_uart, irp)) {
        return iohook_invoke_next(irp);
    }

    EnterCriticalSection(&sg_reader_lock);
    hr = sg_reader_handle_irp_locked(irp);
    LeaveCriticalSection(&sg_reader_lock);

    return hr;
}

static HRESULT sg_reader_handle_irp_locked(struct irp *irp)
{
    HRESULT hr;

#if defined(LOG_NFC)
    if (irp->op == IRP_OP_WRITE) {
        dprintf("WRITE:\n");
        dump_const_iobuf(&irp->write);
    }
#endif

#if defined(LOG_NFC)
    if (irp->op == IRP_OP_READ) {
        dprintf("READ:\n");
        dump_iobuf(&sg_reader_uart.readable);
    }
#endif

    if (irp->op == IRP_OP_OPEN) {
        /* Unfortunately the card reader UART gets opened and closed
           repeatedly */

        if (!sg_reader_started) {
            dprintf("NFC Assembly: Starting backend DLL\n");
            hr = aime_dll.init();

            sg_reader_started = true;
            sg_reader_start_hr = hr;

            if (FAILED(hr)) {
                dprintf("NFC Assembly: Backend error: %x\n", (int) hr);

                return hr;
            }
        } else {
            hr = sg_reader_start_hr;

            if (FAILED(hr)) {
                return hr;
            }
        }
    }

    hr = uart_handle_irp(&sg_reader_uart, irp);

    if (FAILED(hr) || irp->op != IRP_OP_WRITE) {
        return hr;
    }

    sg_nfc_transact(
            &sg_reader_nfc,
            &sg_reader_uart.readable,
            sg_reader_uart.written.bytes,
            sg_reader_uart.written.pos);

    sg_led_transact(
            &sg_reader_led,
            &sg_reader_uart.readable,
            sg_reader_uart.written.bytes,
            sg_reader_uart.written.pos);

    sg_reader_uart.written.pos = 0;

    return hr;
}

static HRESULT sg_reader_nfc_poll(void *ctx)
{
    return aime_dll.nfc_poll(0);
}

static HRESULT sg_reader_nfc_get_aime_id(
        void *ctx,
        uint8_t *luid,
        size_t luid_size)
{
    return aime_dll.nfc_get_aime_id(0, luid, luid_size);
}

static HRESULT sg_reader_nfc_get_felica_id(void *ctx, uint64_t *IDm)
{
    return aime_dll.nfc_get_felica_id(0, IDm);
}

static HRESULT sg_reader_nfc_get_mifare_uid(
        void *ctx,
        uint8_t *uid,
        size_t uid_size)
{
    if (aime_dll.nfc_get_mifare_uid == NULL) {
        return S_FALSE;
    }

    return aime_dll.nfc_get_mifare_uid(0, uid, uid_size);
}

static HRESULT sg_reader_nfc_mifare_select(
        void *ctx,
        const uint8_t *uid,
        size_t uid_size)
{
    if (aime_dll.nfc_mifare_select == NULL) {
        return S_FALSE;
    }

    return aime_dll.nfc_mifare_select(0, uid, uid_size);
}

static HRESULT sg_reader_nfc_mifare_set_key(
        void *ctx,
        uint8_t key_type,
        const uint8_t *key,
        size_t key_size)
{
    if (aime_dll.nfc_mifare_set_key == NULL) {
        return S_FALSE;
    }

    return aime_dll.nfc_mifare_set_key(0, key_type, key, key_size);
}

static HRESULT sg_reader_nfc_mifare_authenticate(
        void *ctx,
        uint8_t key_type,
        const uint8_t *payload,
        size_t payload_size)
{
    if (aime_dll.nfc_mifare_authenticate == NULL) {
        return S_FALSE;
    }

    return aime_dll.nfc_mifare_authenticate(
            0,
            key_type,
            payload,
            payload_size);
}

static HRESULT sg_reader_nfc_mifare_read_block(
        void *ctx,
        const uint8_t *uid,
        size_t uid_size,
        uint8_t block_no,
        uint8_t *block,
        size_t block_size)
{
    if (aime_dll.nfc_mifare_read_block == NULL) {
        return S_FALSE;
    }

    return aime_dll.nfc_mifare_read_block(
            0,
            uid,
            uid_size,
            block_no,
            block,
            block_size);
}

static HRESULT sg_reader_nfc_felica_transact(
        void *ctx,
        const uint8_t *req,
        size_t req_size,
        uint8_t *res,
        size_t res_size,
        size_t *res_size_written)
{
    if (aime_dll.nfc_felica_transact == NULL) {
        return S_FALSE;
    }

    return aime_dll.nfc_felica_transact(
            0,
            req,
            req_size,
            res,
            res_size,
            res_size_written);
}

static HRESULT sg_reader_nfc_radio_on(void *ctx)
{
    if (aime_dll.nfc_radio_on == NULL) {
        return S_FALSE;
    }

    return aime_dll.nfc_radio_on(0);
}

static HRESULT sg_reader_nfc_radio_off(void *ctx)
{
    if (aime_dll.nfc_radio_off == NULL) {
        return S_FALSE;
    }

    return aime_dll.nfc_radio_off(0);
}

static HRESULT sg_reader_nfc_to_update_mode(void *ctx)
{
    if (aime_dll.nfc_to_update_mode == NULL) {
        return S_FALSE;
    }

    return aime_dll.nfc_to_update_mode(0);
}

static HRESULT sg_reader_nfc_send_hex_data(
        void *ctx,
        const uint8_t *payload,
        size_t payload_size,
        uint8_t *status_out)
{
    if (aime_dll.nfc_send_hex_data == NULL) {
        return S_FALSE;
    }

    return aime_dll.nfc_send_hex_data(0, payload, payload_size, status_out);
}

static void sg_reader_led_set_color(void *ctx, uint8_t r, uint8_t g, uint8_t b)
{
    aime_dll.led_set_color(0, r, g, b);
}
