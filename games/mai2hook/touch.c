#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "hooklib/fdshark.h"
#include "hooklib/reg.h"

#include "mai2hook/mai2-dll.h"
#include "mai2hook/touch.h"

#include "util/dprintf.h"
#include "util/dump.h"

static HRESULT touch_handle_irp(struct irp *irp);
static HRESULT touch_handle_irp_locked(
        struct irp *irp,
        struct uart *uart);
static HRESULT touch_handle_read(struct irp *irp, struct uart *uart);
static HRESULT touch_handle_ioctl(struct irp *irp, struct uart *uart);
static void touch_complete_pending_read(struct uart *uart);
static void touch_release_pending_read(struct uart *uart);
static void touch_clear_auto_scan(struct uart *uart);
static bool touch_read_available(struct uart *uart);
static size_t touch_current_depth(struct uart *uart);
static void touch_shift_read(struct uart *uart, struct iobuf *read);
static HRESULT touch_enqueue(
        struct uart *uart,
        CONDITION_VARIABLE *cv,
        const void *bytes,
        size_t nbytes);
static HRESULT touch_enqueue_reply(
        struct uart *uart,
        CONDITION_VARIABLE *cv,
        uint8_t side,
        uint8_t sensor,
        uint8_t command,
        uint8_t value);
static void touch_auto_scan(const uint8_t player, const uint8_t state[7]);

struct touch_pending_read {
    OVERLAPPED *ovl;
    uint8_t *bytes;
    size_t nbytes;
};

enum {
    touch_pending_reads_capacity = 4096,
};

struct touch_pending_reads {
    struct touch_pending_read entries[touch_pending_reads_capacity];
    size_t head;
    size_t count;
};

struct touch_auto_scan_state {
    uint8_t frame[9];
    size_t pos;
    bool valid;
};

enum {
    touch_status_pending = 0x00000103UL,
    touch_status_success = 0x00000000UL,
};


static HRESULT read_reg_touch_1p(void *bytes, uint32_t *nbytes)
{
    return reg_hook_read_wstr(bytes, nbytes, L"COM3");
}

static HRESULT read_reg_touch_2p(void *bytes, uint32_t *nbytes)
{
    return reg_hook_read_wstr(bytes, nbytes, L"COM4");
}

static const struct reg_hook_val touch_reg_key[] = {
    {
        .name = L"\\Device\\RealTouchBoard0",
        .read = read_reg_touch_1p,
        .type = REG_SZ,
    },
    {
        .name = L"\\Device\\RealTouchBoard1",
        .read = read_reg_touch_2p,
        .type = REG_SZ,
    },
};

const char *sensor_map[34] = {
    "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", // 0x41 - 0x48
    "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", // 0x49 - 0x50
    "C1", "C2",                                     // 0x51 - 0x52
    "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", // 0x53 - 0x5A
    "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8"  // 0x5B - 0x62
};

const char *sensor_to_str(uint8_t sensor)
{
    if (sensor < 0x41 || sensor > 0x62)
    {
        return "Invalid";
    }

    return sensor_map[sensor - 0x41];
}

static CRITICAL_SECTION touch_1p_lock;
static CONDITION_VARIABLE touch_1p_cv;
static struct touch_pending_reads touch_1p_pending_reads;
static struct touch_auto_scan_state touch_1p_auto_scan;
static struct uart touch_1p_uart;
static uint8_t touch_1p_written_bytes[64];
static uint8_t touch_1p_readable_bytes[1024];
static bool touch_1p_status = false;

static CRITICAL_SECTION touch_2p_lock;
static CONDITION_VARIABLE touch_2p_cv;
static struct touch_pending_reads touch_2p_pending_reads;
static struct touch_auto_scan_state touch_2p_auto_scan;
static struct uart touch_2p_uart;
static uint8_t touch_2p_written_bytes[64];
static uint8_t touch_2p_readable_bytes[1024];
static bool touch_2p_status = false;

static struct touch_pending_reads *touch_get_pending_reads(struct uart *uart)
{
    return uart->port_no == 3 ? &touch_1p_pending_reads : &touch_2p_pending_reads;
}

static bool *touch_get_status_flag(struct uart *uart)
{
    return uart->port_no == 3 ? &touch_1p_status : &touch_2p_status;
}

static struct touch_auto_scan_state *touch_get_auto_scan(struct uart *uart)
{
    return uart->port_no == 3 ? &touch_1p_auto_scan : &touch_2p_auto_scan;
}

HRESULT touch_hook_init(const struct touch_config *cfg)
{
    assert(cfg != NULL);

    if (!cfg->enable_1p && !cfg->enable_2p)
    {
        return S_FALSE;
    }

    HRESULT hr = reg_hook_push_key(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", touch_reg_key, _countof(touch_reg_key));

    if (FAILED(hr))
    {
        return hr;
    }

    if (cfg->enable_1p)
    {
        dprintf("Mai2 touch 1P: Init.\n");

        InitializeCriticalSection(&touch_1p_lock);
        InitializeConditionVariable(&touch_1p_cv);
        uart_init(&touch_1p_uart, 3);
        touch_1p_uart.written.bytes = touch_1p_written_bytes;
        touch_1p_uart.written.nbytes = sizeof(touch_1p_written_bytes);
        touch_1p_uart.readable.bytes = touch_1p_readable_bytes;
        touch_1p_uart.readable.nbytes = sizeof(touch_1p_readable_bytes);
    }

    if (cfg->enable_2p)
    {
        dprintf("Mai2 touch 2P: Init.\n");

        InitializeCriticalSection(&touch_2p_lock);
        InitializeConditionVariable(&touch_2p_cv);
        uart_init(&touch_2p_uart, 4);
        touch_2p_uart.written.bytes = touch_2p_written_bytes;
        touch_2p_uart.written.nbytes = sizeof(touch_2p_written_bytes);
        touch_2p_uart.readable.bytes = touch_2p_readable_bytes;
        touch_2p_uart.readable.nbytes = sizeof(touch_2p_readable_bytes);
    }

    return iohook_push_handler(touch_handle_irp);
}

static HRESULT touch_handle_irp(struct irp *irp)
{
    HRESULT hr;

    assert(irp != NULL);

    if (uart_match_irp(&touch_1p_uart, irp))
    {
        EnterCriticalSection(&touch_1p_lock);
        hr = touch_handle_irp_locked(irp, &touch_1p_uart);
        LeaveCriticalSection(&touch_1p_lock);
    }
    else if (uart_match_irp(&touch_2p_uart, irp))
    {
        EnterCriticalSection(&touch_2p_lock);
        hr = touch_handle_irp_locked(irp, &touch_2p_uart);
        LeaveCriticalSection(&touch_2p_lock);
    }
    else
    {
        return iohook_invoke_next(irp);
    }

    return hr;
}

static HRESULT touch_handle_irp_locked(
        struct irp *irp,
        struct uart *uart)
{
    HRESULT hr;

    if (irp->op == IRP_OP_OPEN)
    {
        touch_clear_auto_scan(uart);
        touch_release_pending_read(uart);
        dprintf("Mai2 touch port %d: Starting backend\n", uart->port_no);
        hr = mai2_dll.touch_init(touch_auto_scan);

        if (FAILED(hr))
        {
            dprintf("Mai2 touch port %d: Backend error: %x\n", uart->port_no, (int)hr);
            return hr;
        }
    }

    if (irp->op == IRP_OP_READ) {
        return touch_handle_read(irp, uart);
    }

    if (irp->op == IRP_OP_IOCTL) {
        return touch_handle_ioctl(irp, uart);
    }

    hr = uart_handle_irp(uart, irp);

    if (FAILED(hr) || irp->op != IRP_OP_WRITE)
    {
        if (irp->op == IRP_OP_CLOSE) {
            *touch_get_status_flag(uart) = false;
            touch_clear_auto_scan(uart);
            touch_release_pending_read(uart);
        }
        return hr;
    }

#if defined(LOG_MAI2_TOUCH)
    dprintf("Mai2 touch port %d WRITE:\n", uart->port_no);
    dump_iobuf(&uart->written);
#endif

    if (uart->written.pos < 6) {
        dprintf("Mai2 touch port %d: Short write (%u bytes)\n",
                uart->port_no,
                (unsigned int) uart->written.pos);
        uart->written.pos = 0;

        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }

    uint8_t port_no = uart->port_no;
    uint8_t *src = uart->written.bytes;

    switch (src[3])
    {
    case commandRSET:
        dprintf("Mai2 touch port %d: Reset\n", port_no);
        break;

    case commandHALT: // Enter Conditioning mode and stop sending touch data.
        dprintf("Mai2 touch port %d: Halt\n", port_no);
        assert(mai2_dll.touch_update != NULL);
        if (port_no == 3)
        {
            touch_1p_status = false;
            mai2_dll.touch_update(touch_1p_status, touch_2p_status);
        }
        else
        {
            touch_2p_status = false;
            mai2_dll.touch_update(touch_1p_status, touch_2p_status);
        }
        touch_clear_auto_scan(uart);
        touch_release_pending_read(uart);
        break;

    case commandSTAT: // Exit Conditioning mode and resume sending touch data.
        dprintf("Mai2 touch port %d: Stat\n", port_no);
        assert(mai2_dll.touch_update != NULL);
        if (port_no == 3)
        {
            touch_1p_status = true;
            mai2_dll.touch_update(touch_1p_status, touch_2p_status);
        }
        else
        {
            touch_2p_status = true;
            mai2_dll.touch_update(touch_1p_status, touch_2p_status);
        }
        break;

    case commandRatio:
#if defined(LOG_MAI2_TOUCH)
        dprintf("Mai2 touch side %c: set sensor %s ratio to %d\n", src[1], sensor_to_str(src[2]), src[4]);
#endif
        hr = touch_enqueue_reply(
                uart,
                uart == &touch_1p_uart ? &touch_1p_cv : &touch_2p_cv,
                src[1],
                src[2],
                commandRatio,
                src[4]);
        break;

    case commandSens:
#if defined(LOG_MAI2_TOUCH)
        dprintf("Mai2 touch side %c: set sensor %s sensitivity to %d\n", src[1], sensor_to_str(src[2]), src[4]);
#endif
        assert(mai2_dll.touch_set_sens != NULL);
        hr = touch_enqueue_reply(
                uart,
                uart == &touch_1p_uart ? &touch_1p_cv : &touch_2p_cv,
                src[1],
                src[2],
                commandSens,
                src[4]);

        if (SUCCEEDED(hr)) {
            uint8_t sens_bytes[6];

            sens_bytes[0] = res_start;
            sens_bytes[1] = src[1];
            sens_bytes[2] = src[2];
            sens_bytes[3] = commandSens;
            sens_bytes[4] = src[4];
            sens_bytes[5] = res_end;
            mai2_dll.touch_set_sens(sens_bytes);
        }

        break;

    default:
        dprintf("Mai2 touch port %d: Unknow %02x\n", port_no, src[3]);
        break;
    }
#if defined(LOG_MAI2_TOUCH)
    dprintf("Mai2 touch port %d READ:\n", uart->port_no);
    dump_iobuf(&uart->readable);
#endif
    uart->written.pos = 0;

    return hr;
}

static HRESULT touch_handle_read(struct irp *irp, struct uart *uart)
{
    struct touch_pending_reads *pending_reads;
    struct touch_pending_read *pending;
    bool *status;
    size_t tail;

    pending_reads = touch_get_pending_reads(uart);
    status = touch_get_status_flag(uart);

    if (!touch_read_available(uart)) {
        if (irp->ovl != NULL &&
            *status &&
            pending_reads->count < touch_pending_reads_capacity) {
            tail = (pending_reads->head + pending_reads->count) %
                    touch_pending_reads_capacity;
            pending = &pending_reads->entries[tail];
            pending_reads->count++;

            pending->ovl = irp->ovl;
            pending->bytes = irp->read.bytes;
            pending->nbytes = irp->read.nbytes;
            pending->ovl->Internal = touch_status_pending;
            pending->ovl->InternalHigh = 0;

            if (pending->ovl->hEvent != NULL) {
                ResetEvent(pending->ovl->hEvent);
            }

            return HRESULT_FROM_WIN32(ERROR_IO_PENDING);
        }
        return E_PENDING;
    }

    touch_shift_read(uart, &irp->read);

    return S_OK;
}

static HRESULT touch_handle_ioctl(struct irp *irp, struct uart *uart)
{
    if (irp->ioctl == IOCTL_SERIAL_GET_COMMSTATUS) {
        uart->status.AmountInInQueue = (ULONG) touch_current_depth(uart);
        uart->status.AmountInOutQueue = uart->written.pos;

        return iobuf_write(&irp->read, &uart->status, sizeof(uart->status));
    }

    return uart_handle_irp(uart, irp);
}

static void touch_complete_pending_read(struct uart *uart)
{
    struct touch_pending_reads *pending_reads;
    struct touch_pending_read *pending;
    struct iobuf read;
    OVERLAPPED *ovl;
    HANDLE event;

    pending_reads = touch_get_pending_reads(uart);

    if (pending_reads->count == 0 || !touch_read_available(uart)) {
        return;
    }

    pending = &pending_reads->entries[pending_reads->head];

    read.bytes = pending->bytes;
    read.nbytes = pending->nbytes;
    read.pos = 0;

    touch_shift_read(uart, &read);

    ovl = pending->ovl;
    memset(pending, 0, sizeof(*pending));
    pending_reads->head = (pending_reads->head + 1) %
            touch_pending_reads_capacity;
    pending_reads->count--;

    ovl->InternalHigh = (ULONG_PTR) read.pos;

    event = ovl->hEvent;
    MemoryBarrier();
    ovl->Internal = touch_status_success;

    if (event != NULL) {
        SetEvent(event);
    }
}

static void touch_release_pending_read(struct uart *uart)
{
    struct touch_pending_reads *pending_reads;
    struct touch_pending_read *pending;
    OVERLAPPED *ovl;
    HANDLE event;
    size_t i;

    pending_reads = touch_get_pending_reads(uart);

    for (i = 0; i < pending_reads->count; i++) {
        pending = &pending_reads->entries[
                (pending_reads->head + i) % touch_pending_reads_capacity];

        if (pending->ovl == NULL) {
            continue;
        }

        ovl = pending->ovl;
        ovl->InternalHigh = 0;
        event = ovl->hEvent;
        MemoryBarrier();
        ovl->Internal = touch_status_success;

        if (event != NULL) {
            SetEvent(event);
        }
    }

    memset(pending_reads, 0, sizeof(*pending_reads));
}

static void touch_clear_auto_scan(struct uart *uart)
{
    struct touch_auto_scan_state *auto_scan;

    auto_scan = touch_get_auto_scan(uart);
    auto_scan->pos = 0;
    auto_scan->valid = false;
}

static bool touch_read_available(struct uart *uart)
{
    return uart->readable.pos > 0 || touch_get_auto_scan(uart)->valid;
}

static size_t touch_current_depth(struct uart *uart)
{
    size_t depth;
    struct touch_auto_scan_state *auto_scan;

    depth = uart->readable.pos;
    auto_scan = touch_get_auto_scan(uart);

    if (auto_scan->valid) {
        depth += sizeof(auto_scan->frame) - auto_scan->pos;
    }

    return depth;
}

static void touch_shift_read(struct uart *uart, struct iobuf *read)
{
    struct touch_auto_scan_state *auto_scan;
    size_t read_avail;
    size_t frame_avail;
    size_t chunksz;

    if (uart->readable.pos > 0) {
        iobuf_shift(read, &uart->readable);
        return;
    }

    auto_scan = touch_get_auto_scan(uart);

    if (!auto_scan->valid) {
        return;
    }

    read_avail = read->nbytes - read->pos;
    frame_avail = sizeof(auto_scan->frame) - auto_scan->pos;
    chunksz = read_avail < frame_avail ? read_avail : frame_avail;

    memcpy(&read->bytes[read->pos],
            &auto_scan->frame[auto_scan->pos],
            chunksz);
    read->pos += chunksz;
    auto_scan->pos += chunksz;

    if (auto_scan->pos == sizeof(auto_scan->frame)) {
        auto_scan->pos = 0;
        auto_scan->valid = false;
    }
}

static HRESULT touch_enqueue(
        struct uart *uart,
        CONDITION_VARIABLE *cv,
        const void *bytes,
        size_t nbytes)
{
    HRESULT hr;

    hr = iobuf_write(&uart->readable, bytes, nbytes);

    if (FAILED(hr)) {
        dprintf("Mai2 touch port %d: RX queue overflow (%u/%u bytes)\n",
                uart->port_no,
                (unsigned int) uart->readable.pos,
                (unsigned int) uart->readable.nbytes);
    } else {
        while (touch_read_available(uart) &&
               touch_get_pending_reads(uart)->count > 0) {
            touch_complete_pending_read(uart);
        }
        WakeConditionVariable(cv);
    }

    return hr;
}

static HRESULT touch_enqueue_reply(
        struct uart *uart,
        CONDITION_VARIABLE *cv,
        uint8_t side,
        uint8_t sensor,
        uint8_t command,
        uint8_t value)
{
    uint8_t reply[6];

    reply[0] = res_start;
    reply[1] = side;
    reply[2] = sensor;
    reply[3] = command;
    reply[4] = value;
    reply[5] = res_end;

    return touch_enqueue(uart, cv, reply, sizeof(reply));
}

static void touch_auto_scan(const uint8_t player, const uint8_t state[7])
{
    struct touch_auto_scan_state *auto_scan;
    struct uart *touch_uart;
    CRITICAL_SECTION *touch_lock;
    CONDITION_VARIABLE *touch_cv;
    uint8_t frame[9];

    if (player == 1) {
        touch_uart = &touch_1p_uart;
        touch_lock = &touch_1p_lock;
        touch_cv = &touch_1p_cv;
    } else {
        touch_uart = &touch_2p_uart;
        touch_lock = &touch_2p_lock;
        touch_cv = &touch_2p_cv;
    }

    if (touch_uart->readable.bytes == NULL) {
        return;
    }

    frame[0] = res_start;
    memcpy(&frame[1], state, 7);
    frame[8] = res_end;

    EnterCriticalSection(touch_lock);
    if (!*touch_get_status_flag(touch_uart)) {
        LeaveCriticalSection(touch_lock);
        return;
    }
    auto_scan = touch_get_auto_scan(touch_uart);
    memcpy(auto_scan->frame, frame, sizeof(frame));
    auto_scan->pos = 0;
    auto_scan->valid = true;
    while (touch_read_available(touch_uart) &&
           touch_get_pending_reads(touch_uart)->count > 0) {
        touch_complete_pending_read(touch_uart);
    }
    WakeConditionVariable(touch_cv);
    LeaveCriticalSection(touch_lock);
}
