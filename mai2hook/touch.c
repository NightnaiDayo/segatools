#include "mai2hook/touch.h"

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

#define PORT_1P 3
#define PORT_2P 4

static CRITICAL_SECTION touch_1p_lock;
static struct uart touch_1p_uart;
static uint8_t touch_1p_written_bytes[64];
static uint8_t touch_1p_readable_bytes[64];

static CRITICAL_SECTION touch_2p_lock;
static struct uart touch_2p_uart;
static uint8_t touch_2p_written_bytes[64];
static uint8_t touch_2p_readable_bytes[64];

HRESULT touch_hook_init(const struct touch_config *cfg)
{
    HRESULT hr;

    assert(cfg != NULL);

    if (!cfg->enable_1p && !cfg->enable_2p)
    {
        return S_FALSE;
    }

    hr = reg_hook_push_key(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", touch_reg_key, _countof(touch_reg_key));

    if (FAILED(hr))
    {
        return hr;
    }

    if (cfg->enable_1p)
    {
        dprintf("Mai2 touch port %d: Init.\n", PORT_1P);

        InitializeCriticalSection(&touch_1p_lock);
        uart_init(&touch_1p_uart, PORT_1P);
        touch_1p_uart.written.bytes = touch_1p_written_bytes;
        touch_1p_uart.written.nbytes = sizeof(touch_1p_written_bytes);
        touch_1p_uart.readable.bytes = touch_1p_readable_bytes;
        touch_1p_uart.readable.nbytes = sizeof(touch_1p_readable_bytes);
    }

    if (cfg->enable_2p)
    {
        dprintf("Mai2 touch port %d: Init.\n", PORT_2P);

        InitializeCriticalSection(&touch_2p_lock);
        uart_init(&touch_2p_uart, PORT_2P);
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

static HRESULT touch_handle_irp_locked(struct irp *irp, struct uart *uart)
{
    HRESULT hr;

    if (irp->op == IRP_OP_OPEN)
    {
        dprintf("Mai2 touch port %d: Starting backend\n", uart->port_no);
        hr = mai2_dll.touch_init();

        if (FAILED(hr))
        {
            dprintf("Mai2 touch port %d: Backend error: %x\n", uart->port_no, (int)hr);
            return hr;
        }
    }

    hr = uart_handle_irp(uart, irp);

    if (FAILED(hr) || irp->op != IRP_OP_WRITE)
    {
        return hr;
    }

#if defined(LOG_MAI2_TOUCH)
    dprintf("Mai2 touch port %d WRITE:\n", uart->port_no);
    dump_iobuf(&uart->written);
#endif
    uint8_t port_no = uart->port_no;
    uint8_t *src = uart->written.bytes;
    uint8_t *dest = uart->readable.bytes;

    switch (src[3])
    {
    case commandRSET:
        dprintf("Mai2 touch port %d: Reset\n", port_no);
        break;

    case commandHALT: // Enter Conditioning mode and stop sending touch data.
        dprintf("Mai2 touch port %d: Halt\n", port_no);
        if (port_no == PORT_1P)
        {
            assert(mai2_dll.touch_1p_stop != NULL);
            LeaveCriticalSection(&touch_1p_lock);
            mai2_dll.touch_1p_stop();
            EnterCriticalSection(&touch_1p_lock);
        }
        else if (port_no == PORT_2P)
        {
            assert(mai2_dll.touch_2p_stop != NULL);
            LeaveCriticalSection(&touch_2p_lock);
            mai2_dll.touch_2p_stop();
            EnterCriticalSection(&touch_2p_lock);
        }
        break;

    case commandSTAT: // Exit Conditioning mode and resume sending touch data.
        dprintf("Mai2 touch port %d: Stat\n", port_no);
        if (port_no == PORT_1P)
        {
            assert(mai2_dll.touch_1p_start != NULL);
            mai2_dll.touch_1p_start(touch_1p_auto_scan);
        }
        else if (port_no == PORT_2P)
        {
            assert(mai2_dll.touch_2p_start != NULL);
            mai2_dll.touch_2p_start(touch_2p_auto_scan);
        }
        break;

    case commandRatio:
#if defined(LOG_MAI2_TOUCH)
        dprintf("Mai2 touch side %c: set sensor %s ratio to %d\n", src[1], sensor_to_str(src[2]), src[4]);
#endif
        dest[0] = res_start;
        dest[1] = src[1]; // L,R
        dest[2] = src[2]; // sensor
        dest[3] = commandRatio;
        dest[4] = src[4]; // Ratio
        dest[5] = res_end;
        uart->readable.pos = 6;
        // The Ratio is fixed at 0x72 and does not need to be sent to mai2io for processing.
        break;

    case commandSens:
#if defined(LOG_MAI2_TOUCH)
        dprintf("Mai2 touch side %c: set sensor %s sensitivity to %d\n", src[1], sensor_to_str(src[2]), src[4]);
#endif
        dest[0] = res_start;
        dest[1] = src[1]; // L,R
        dest[2] = src[2]; // sensor
        dest[3] = commandSens;
        dest[4] = src[4]; // Sensitivity
        dest[5] = res_end;
        uart->readable.pos = 6;
        mai2_dll.touch_set_sens(dest);
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

static void touch_1p_auto_scan(const uint8_t *state)
{
    memcpy(touch_1p_uart.readable.bytes, state, 9);
    touch_1p_uart.readable.pos = 9;
}

static void touch_2p_auto_scan(const uint8_t *state)
{
    memcpy(touch_2p_uart.readable.bytes, state, 9);
    touch_2p_uart.readable.pos = 9;
}