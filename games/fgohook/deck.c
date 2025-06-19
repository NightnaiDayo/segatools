/*
    SEGA 837-15345 RFID Deck Reader emulator

    Credits:

    OLEG
    Coburn
    Mitsuhide
*/

#include <windows.h>

#include <assert.h>

#include "board/sg-frame.h"

#include "fgohook/deck.h"

#include "hook/iobuf.h"
#include "hook/iohook.h"

#include "hooklib/uart.h"

#include "util/dprintf.h"
#include "util/dump.h"

#define MAX_CARDS 30

// request format:
// 0xe0 - sync
// 0x?? - command
// 0x?? - payload length
// ...  - payload
// 0x?? - checksum (sum of everything except the sync byte)
//
// response format:
// 0xe0 - sync
// 0x?? - command
// 0x?? - status code
// 0x?? - payload length
// ...  - payload
// 0x?? - checksum

enum {
    DECK_CMD_RESET = 0x41,
    DECK_CMD_GET_BOOT_FW_VERSION = 0x84,
    DECK_CMD_GET_BOARD_INFO = 0x85,
    DECK_CMD_INIT_UNK1 = 0x81,
    DECK_CMD_GET_APP_FW_VERSION = 0x42,
    DECK_CMD_INIT_UNK2 = 0x04,
    DECK_CMD_INIT_UNK3 = 0x05,
    DECK_CMD_READ = 0x06
};

enum {
    DECK_READ_START = 0x81,
    DECK_READ_DATA = 0x82,
    DECK_READ_END = 0x83
};

struct deck_hdr {
    uint8_t sync;
    uint8_t cmd;
    uint8_t nbytes;
};

struct deck_resp_hdr {
    uint8_t sync;
    uint8_t cmd;
    uint8_t status;
    uint8_t nbytes;
};

struct deck_resp_get_boot_fw_version {
    struct deck_resp_hdr hdr;
    uint8_t boot_fw_version;
};

struct deck_resp_get_app_fw_version {
    struct deck_resp_hdr hdr;
    uint8_t app_fw_version;
};

struct deck_resp_get_board_info {
    struct deck_resp_hdr hdr;
    uint8_t board[9];
};

struct deck_resp_read {
    struct deck_resp_hdr hdr;
    uint8_t card_data[44];
};

union deck_req_any {
    struct deck_hdr hdr;
    uint8_t bytes[520];
};

struct card_collection {
    uint8_t nCards;
    uint8_t cards[MAX_CARDS][44];
};

static HRESULT init_mmf(void);

static HRESULT deck_handle_irp(struct irp *irp);
static HRESULT deck_handle_irp_locked(struct irp *irp);
static HRESULT deck_req_dispatch(const union deck_req_any* req);
static HRESULT deck_req_get_boot_fw_version(void);
static HRESULT deck_req_get_app_fw_version(void);
static HRESULT deck_req_get_board_info(void);
static HRESULT deck_req_read(void);
static HRESULT deck_req_nop(uint8_t cmd);
static void deck_read_one(void);

static HRESULT deck_frame_accept(const struct iobuf* dest);
static void deck_frame_sync(struct iobuf* src);
static HRESULT deck_frame_decode(struct iobuf *dest, struct iobuf *src);
static HRESULT deck_frame_encode(struct iobuf* dest, const void* ptr, size_t nbytes);
static HRESULT deck_frame_encode_byte(struct iobuf* dest, uint8_t byte);

static CRITICAL_SECTION deck_lock;
static struct uart deck_uart;
static uint8_t deck_written_bytes[1024];
static uint8_t deck_readable_bytes[1024];
static HANDLE mutex;
static HANDLE mmf;
static struct card_collection* cards_ptr;
static uint8_t current_card_idx = 0;
static bool read_pending = false;

HRESULT deck_hook_init(const struct deck_config *cfg, unsigned int port_no)
{
    assert(cfg != NULL);

    if (!cfg->enable) {
        return S_FALSE;
    }

    InitializeCriticalSection(&deck_lock);

    uart_init(&deck_uart, port_no);
    deck_uart.written.bytes = deck_written_bytes;
    deck_uart.written.nbytes = sizeof(deck_written_bytes);
    deck_uart.readable.bytes = deck_readable_bytes;
    deck_uart.readable.nbytes = sizeof(deck_readable_bytes);

    if (FAILED(init_mmf())) {
        return S_FALSE;
    }

    dprintf("Deck Reader: hook enabled.\n");

    return iohook_push_handler(deck_handle_irp);
}

static HRESULT init_mmf(void) {
    mutex = CreateMutexA(NULL, FALSE, "FGODeckMutex");
    if (mutex == NULL) {
        return S_FALSE;
    }

    mmf = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, MAX_CARDS * 44 + 1, "FGODeck");
    if (mmf == NULL) {
        return S_FALSE;
    }

    cards_ptr = (struct card_collection*)MapViewOfFile(mmf, FILE_MAP_ALL_ACCESS, 0, 0, MAX_CARDS * 44 + 1);

    return S_OK;
}

static HRESULT deck_handle_irp(struct irp *irp)
{
    HRESULT hr;

    assert(irp != NULL);

    if (!uart_match_irp(&deck_uart, irp)) {
        return iohook_invoke_next(irp);
    }

    EnterCriticalSection(&deck_lock);
    hr = deck_handle_irp_locked(irp);
    LeaveCriticalSection(&deck_lock);

    return hr;
}

static HRESULT deck_handle_irp_locked(struct irp *irp)
{
    uint8_t req[1024];
    struct iobuf req_iobuf;
    HRESULT hr;

    if (irp->op == IRP_OP_OPEN) {
        dprintf("Deck Reader: Starting backend\n");
    }

    hr = uart_handle_irp(&deck_uart, irp);

    if (SUCCEEDED(hr) && irp->op == IRP_OP_READ && read_pending == true) {
        deck_read_one();
        return S_OK;
    }

    if (FAILED(hr) || irp->op != IRP_OP_WRITE) {
        return hr;
    }

    for (;;) {
        // if (deck_uart.written.pos != 0) {
        //     dprintf("Deck Reader: TX Buffer:\n");
        //     dump_iobuf(&deck_uart.written);
        // }

        req_iobuf.bytes = req;
        req_iobuf.nbytes = sizeof(req);
        req_iobuf.pos = 0;

        hr = deck_frame_decode(&req_iobuf, &deck_uart.written);

        if (hr != S_OK) {
            if (FAILED(hr)) {
                dprintf("Deck Reader: Deframe error: %x, %d %d\n", (int) hr, (int) req_iobuf.nbytes, (int) req_iobuf.pos);
            }

            return hr;
        }

        // dprintf("Deck Reader: Deframe Buffer:\n");
        // dump_iobuf(&req_iobuf);

        hr = deck_req_dispatch((const union deck_req_any *) &req);

        if (FAILED(hr)) {
            dprintf("Deck Reader: Processing error: %x\n", (int) hr);
        }

        // dprintf("Deck Reader: Written bytes:\n");
        // dump_iobuf(&deck_uart.readable);
    }
}

static HRESULT deck_req_dispatch(const union deck_req_any *req) {
    switch (req->hdr.cmd) {
    case DECK_CMD_RESET:
    case DECK_CMD_INIT_UNK1:
    case DECK_CMD_INIT_UNK2:
    case DECK_CMD_INIT_UNK3:
        return deck_req_nop(req->hdr.cmd);

    case DECK_CMD_GET_BOOT_FW_VERSION:
        return deck_req_get_boot_fw_version();

    case DECK_CMD_GET_APP_FW_VERSION:
        return deck_req_get_app_fw_version();

    case DECK_CMD_GET_BOARD_INFO:
        return deck_req_get_board_info();

    case DECK_CMD_READ:
        return deck_req_read();

    default:
        dprintf("Deck Reader: Unhandled command %#02x\n", req->hdr.cmd);

        return S_OK;
    }
}

static HRESULT deck_req_get_boot_fw_version(void) {
    struct deck_resp_get_boot_fw_version resp;

    dprintf("Deck Reader: Get Boot FW Version\n");

    resp.hdr.sync = 0xE0;
    resp.hdr.cmd = DECK_CMD_GET_BOOT_FW_VERSION;
    resp.hdr.status = 0;
    resp.hdr.nbytes = 1;
    resp.boot_fw_version = 0x90;

    return deck_frame_encode(&deck_uart.readable, &resp, sizeof(resp));
}

static HRESULT deck_req_get_app_fw_version(void) {
    struct deck_resp_get_app_fw_version resp;

    dprintf("Deck Reader: Get App FW Version\n");

    resp.hdr.sync = 0xE0;
    resp.hdr.cmd = DECK_CMD_GET_APP_FW_VERSION;
    resp.hdr.status = 0;
    resp.hdr.nbytes = 1;
    resp.app_fw_version = 0x91;

    return deck_frame_encode(&deck_uart.readable, &resp, sizeof(resp));
}

static HRESULT deck_req_get_board_info(void) {
    struct deck_resp_get_board_info resp;

    dprintf("Deck Reader: Get Board Info\n");

    resp.hdr.sync = 0xE0;
    resp.hdr.cmd = DECK_CMD_GET_BOARD_INFO;
    resp.hdr.status = 0;
    resp.hdr.nbytes = 9;
    memcpy(resp.board, (void*)"837-15345", 9);

    return deck_frame_encode(&deck_uart.readable, &resp, sizeof(resp));
}

static HRESULT deck_req_read(void) {
    struct deck_resp_read resp;

    dprintf("Deck Reader: Read Card\n");

    resp.hdr.sync = 0xE0;
    resp.hdr.cmd = DECK_CMD_READ;
    resp.hdr.status = DECK_READ_START;
    resp.hdr.nbytes = 0;

    ReleaseMutex(mutex);
    WaitForSingleObject(mutex, INFINITE);
    current_card_idx = 0;
    read_pending = true;

    return deck_frame_encode(&deck_uart.readable, &resp.hdr, sizeof(resp.hdr));
}

static void deck_read_one(void) {
    struct deck_resp_read resp;

    resp.hdr.sync = 0xE0;
    resp.hdr.cmd = DECK_CMD_READ;

    if (current_card_idx < cards_ptr->nCards) {
        resp.hdr.status = DECK_READ_DATA;
        resp.hdr.nbytes = 44;
        memcpy(resp.card_data, cards_ptr->cards[current_card_idx], 44);
        dump(resp.card_data, 44);

        deck_frame_encode(&deck_uart.readable, &resp, sizeof(resp));
        current_card_idx++;
    } else {
        resp.hdr.status = DECK_READ_END;
        resp.hdr.nbytes = 0;
        deck_frame_encode(&deck_uart.readable, &resp.hdr, sizeof(resp.hdr));
        read_pending = false;
        ReleaseMutex(mutex);
    }
}

static HRESULT deck_req_nop(uint8_t cmd) {
    struct deck_resp_hdr resp;

    dprintf("Deck Reader: No-op cmd %#02x\n", cmd);

    resp.sync = 0xE0;
    resp.cmd = cmd;
    resp.status = 0;
    resp.nbytes = 0;

    return deck_frame_encode(&deck_uart.readable, &resp, sizeof(resp));
}


static void deck_frame_sync(struct iobuf* src)
{
    size_t i;

    for (i = 0; i < src->pos && src->bytes[i] != 0xE0; i++);

    src->pos -= i;
    memmove(&src->bytes[0], &src->bytes[i], i);
}

static HRESULT deck_frame_accept(const struct iobuf* dest)
{
    if (dest->pos < 2 || dest->pos != dest->bytes[2] + 4) {
        return S_FALSE;
    }

    return S_OK;
}

static HRESULT deck_frame_decode(struct iobuf *dest, struct iobuf *src) {
    uint8_t byte;
    bool escape;
    size_t i;
    HRESULT hr;

    assert(dest != NULL);
    assert(dest->bytes != NULL || dest->nbytes == 0);
    assert(dest->pos <= dest->nbytes);
    assert(src != NULL);
    assert(src->bytes != NULL || src->nbytes == 0);
    assert(src->pos <= src->nbytes);

    deck_frame_sync(src);

    dest->pos = 0;
    escape = false;

    for (i = 0, hr = S_FALSE; i < src->pos && hr == S_FALSE; i++) {
        /* Step the FSM to unstuff another byte */

        byte = src->bytes[i];

        if (dest->pos >= dest->nbytes) {
            hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        }
        else if (i == 0) {
            dest->bytes[dest->pos++] = byte;
        }
        else if (byte == 0xE0) {
            hr = E_FAIL;
        }
        else if (byte == 0xD0) {
            if (escape) {
                hr = E_FAIL;
            }

            escape = true;
        }
        else if (escape) {
            dest->bytes[dest->pos++] = byte + 1;
            escape = false;
        }
        else {
            dest->bytes[dest->pos++] = byte;
        }

        /* Try to accept the packet we've built up so far */

        if (SUCCEEDED(hr)) {
            hr = deck_frame_accept(dest);
        }
    }

    /* Handle FSM terminal state */

    if (hr != S_FALSE) {
        /* Frame was either accepted or rejected, remove it from src */
        memmove(&src->bytes[0], &src->bytes[i], src->pos - i);
        src->pos -= i;
    }

    return hr;
}

static HRESULT deck_frame_encode(
    struct iobuf* dest,
    const void* ptr,
    size_t nbytes)
{
    const uint8_t* src;
    uint8_t checksum;
    uint8_t byte;
    size_t i;
    HRESULT hr;

    assert(dest != NULL);
    assert(dest->bytes != NULL || dest->nbytes == 0);
    assert(dest->pos <= dest->nbytes);
    assert(ptr != NULL);

    src = ptr;

    assert(nbytes >= 2 && src[0] == 0xE0 && src[3] + 4 == nbytes);

    if (dest->pos >= dest->nbytes) {
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }

    dest->bytes[dest->pos++] = 0xE0;
    checksum = 0x0;

    for (i = 1; i < nbytes; i++) {
        byte = src[i];
        checksum += byte;

        hr = deck_frame_encode_byte(dest, byte);

        if (FAILED(hr)) {
            return hr;
        }
    }

    return deck_frame_encode_byte(dest, checksum);
}

static HRESULT deck_frame_encode_byte(struct iobuf* dest, uint8_t byte)
{
    if (byte == 0xD0 || byte == 0xE0) {
        if (dest->pos + 2 > dest->nbytes) {
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        }

        dest->bytes[dest->pos++] = 0xD0;
        dest->bytes[dest->pos++] = byte - 1;
    }
    else {
        if (dest->pos + 1 > dest->nbytes) {
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        }

        dest->bytes[dest->pos++] = byte;
    }

    return S_OK;
}
