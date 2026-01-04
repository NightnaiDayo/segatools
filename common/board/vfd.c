/* This is some sort of LCD display found on various cabinets. It is driven
   directly by amdaemon, and it has something to do with displaying the status
   of electronic payments.

   Part number in schematics is "VFD GP1232A02A FUTABA". */

#include <windows.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "board/config.h"
#include "board/aime-dll.h"
#include "board/vfd.h"
#include "board/vfd-cmd.h"

#include "hook/iohook.h"

#include "hooklib/uart.h"

#include "util/dprintf.h"
#include "util/dump.h"

#define SUPER_VERBOSE 0
#define VFD_BRIGHTNESS_MAX 4

static HRESULT vfd_handle_irp(struct irp *irp);

static struct uart vfd_uart;
static uint8_t vfd_written[4096];
static uint8_t vfd_readable[4096];

static struct aime_io_vfd_state vfd_state;

HRESULT vfd_handle_get_version(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_reset(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_clear_screen(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_set_brightness(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_set_screen_on(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_set_h_scroll(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_draw_image(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_set_cursor(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_set_encoding(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_set_text_wnd(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_set_text_speed(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_write_text(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_enable_scroll(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_disable_scroll(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_rotate(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_create_char(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);
HRESULT vfd_handle_create_char2(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart);

static bool utf_enabled;

static void vfd_publish_state(void)
{
    if (aime_dll.vfd_set_state != NULL) {
        aime_dll.vfd_set_state(&vfd_state);
    }
}

HRESULT vfd_hook_init(struct vfd_config *cfg, unsigned int default_port_no)
{
    if (!cfg->enable){
        return S_FALSE;
    }

    utf_enabled = cfg->utf_conversion;
    memset(&vfd_state, 0, sizeof(vfd_state));
    vfd_state.encoding = VFD_ENC_SHIFT_JIS;
    vfd_publish_state();

    unsigned int port_no = cfg->port_no;
    if (port_no == 0){
        port_no = default_port_no;
    }

    dprintf("VFD: enabling (port=%d)\n", port_no);
    uart_init(&vfd_uart, port_no);
    vfd_uart.written.bytes = vfd_written;
    vfd_uart.written.nbytes = sizeof(vfd_written);
    vfd_uart.readable.bytes = vfd_readable;
    vfd_uart.readable.nbytes = sizeof(vfd_readable);

    return iohook_push_handler(vfd_handle_irp);
}


const char* get_encoding_name(int b){
    switch (b){
        case 0: return "gb2312";
        case 1: return "big5";
        case 2: return "shift-jis";
        case 3: return "ks_c_5601-1987";
        default: return "unknown";
    }
}

void print_vfd_text(const char* str, int len){

    if (utf_enabled){

        wchar_t encoded[1024];
        memset(encoded, 0, 1024 * sizeof(wchar_t));

        int codepage = 0;
        if (vfd_state.encoding == VFD_ENC_GB2312){
            codepage = 936;
        } else if (vfd_state.encoding == VFD_ENC_BIG5){
            codepage = 950;
        } else if (vfd_state.encoding == VFD_ENC_SHIFT_JIS){
            codepage = 932;
        } else if (vfd_state.encoding == VFD_ENC_KSC5601) {
            codepage = 949;
        }

        if (!MultiByteToWideChar(codepage, MB_USEGLYPHCHARS, str, len, encoded, 1024)){
            dprintf("VFD: Text conversion failed: %ld", GetLastError());
            return;
        }

        dprintf("VFD: Text: %ls\n", encoded);
    } else {

        dprintf("VFD: Text: %s\n", str);

    }

    if (aime_dll.vfd_set_text != NULL) {
        aime_dll.vfd_set_text((const uint8_t *) str, len, &vfd_state);
    }
}

static void vfd_read_text_until_sync(struct const_iobuf *reader)
{
    int len;

    if (reader->pos >= reader->nbytes) {
        return;
    }

    len = 0;
    while (reader->pos + len < reader->nbytes &&
            reader->bytes[reader->pos + len] != VFD_SYNC_BYTE &&
            reader->bytes[reader->pos + len] != VFD_SYNC_BYTE2) {
        len++;
    }

    if (len <= 0) {
        return;
    }

    char *str = malloc((size_t) len + 1);
    memset(str, 0, (size_t) len + 1);
    iobuf_read(reader, str, len);
    str[len] = '\0';
    print_vfd_text(str, len);
    free(str);
}

static HRESULT vfd_handle_irp(struct irp *irp)
{
    HRESULT hr;

    assert(irp != NULL);

    if (!uart_match_irp(&vfd_uart, irp)) {
        return iohook_invoke_next(irp);
    }

    if (irp->op == IRP_OP_OPEN){
        dprintf("VFD: Open\n");
    } else if (irp->op == IRP_OP_CLOSE){
        dprintf("VFD: Close\n");
    }

    hr = uart_handle_irp(&vfd_uart, irp);

    if (FAILED(hr) || irp->op != IRP_OP_WRITE) {
        return hr;
    }

#if SUPER_VERBOSE
    dprintf("VFD TX:\n");
    dump_iobuf(&vfd_uart.written);
#endif

    struct const_iobuf reader;
    iobuf_flip(&reader, &vfd_uart.written);

    struct iobuf* writer = &vfd_uart.readable;
    for (; reader.pos < reader.nbytes ; ){

        if (vfd_frame_sync(&reader)) {

            reader.pos++; // get the sync byte out of the way

            uint8_t cmd;
            iobuf_read_8(&reader, &cmd);

            if (cmd == VFD_CMD_GET_VERSION) {
                hr = vfd_handle_get_version(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_RESET) {
                hr = vfd_handle_reset(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_CLEAR_SCREEN) {
                hr = vfd_handle_clear_screen(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_SET_BRIGHTNESS) {
                hr = vfd_handle_set_brightness(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_SET_SCREEN_ON) {
                hr = vfd_handle_set_screen_on(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_SET_H_SCROLL) {
                hr = vfd_handle_set_h_scroll(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_DRAW_IMAGE) {
                hr = vfd_handle_draw_image(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_SET_CURSOR) {
                hr = vfd_handle_set_cursor(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_SET_ENCODING) {
                hr = vfd_handle_set_encoding(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_SET_TEXT_WND) {
                hr = vfd_handle_set_text_wnd(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_SET_TEXT_SPEED) {
                hr = vfd_handle_set_text_speed(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_WRITE_STATIC) {
                dprintf("VFD: Write Static Text\n");
                vfd_read_text_until_sync(&reader);
                hr = S_FALSE;
            } else if (cmd == VFD_CMD_WRITE_TEXT) {
                hr = vfd_handle_write_text(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_ENABLE_SCROLL) {
                hr = vfd_handle_enable_scroll(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_DISABLE_SCROLL) {
                hr = vfd_handle_disable_scroll(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_ROTATE) {
                hr = vfd_handle_rotate(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_CREATE_CHAR) {
                hr = vfd_handle_create_char(&reader, writer, &vfd_uart);
            } else if (cmd == VFD_CMD_CREATE_CHAR2) {
                hr = vfd_handle_create_char2(&reader, writer, &vfd_uart);
            } else {
                dprintf("VFD: Unknown command 0x%x\n", cmd);
                dump_const_iobuf(&reader);
                hr = S_FALSE;
            }
        } else {

            // if no sync byte is sent, we are just getting plain text...

            vfd_read_text_until_sync(&reader);

        }

        if (!SUCCEEDED(hr)){
            return hr;
        }

    }

    vfd_uart.written.pos = 0;

    return hr;
}

HRESULT vfd_handle_get_version(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    uint8_t subcmd;

    if (reader->pos < reader->nbytes &&
            reader->bytes[reader->pos] != VFD_SYNC_BYTE &&
            reader->bytes[reader->pos] != VFD_SYNC_BYTE2) {
        iobuf_read_8(reader, &subcmd);
        dprintf("VFD: Get Version (0x%02x)\n", subcmd);
    } else {
        dprintf("VFD: Get Version\n");
    }

    struct vfd_resp_board_info resp;

    memset(&resp, 0, sizeof(resp));
    resp.unk1 = 2;
    strcpy(resp.version, "01.20");
    resp.unk2 = 1;

    return vfd_frame_encode(writer, &resp, sizeof(resp));
}
HRESULT vfd_handle_reset(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    dprintf("VFD: Reset\n");

    memset(&vfd_state, 0, sizeof(vfd_state));
    vfd_state.encoding = VFD_ENC_SHIFT_JIS;
    vfd_publish_state();

    return S_FALSE;
}
HRESULT vfd_handle_clear_screen(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    if (reader->pos + 1 <= reader->nbytes) {
        uint8_t next = reader->bytes[reader->pos];
        bool next_is_sync = (next == VFD_SYNC_BYTE || next == VFD_SYNC_BYTE2);

        if (!next_is_sync && next <= VFD_BRIGHTNESS_MAX) {
            bool end_or_sync = (reader->pos + 1 >= reader->nbytes);

            if (!end_or_sync) {
                uint8_t follow = reader->bytes[reader->pos + 1];
                end_or_sync = (follow == VFD_SYNC_BYTE || follow == VFD_SYNC_BYTE2);
            }

            if (end_or_sync) {
                uint8_t b;
                iobuf_read_8(reader, &b);
                dprintf("VFD: Brightness (compat), %d\n", b);
                vfd_state.brightness = b;
                vfd_publish_state();
                return S_FALSE;
            }
        }
    }

    dprintf("VFD: Clear Screen\n");
    vfd_state.clear_seq++;
    vfd_publish_state();

    return S_FALSE;
}
HRESULT vfd_handle_set_brightness(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    uint8_t b;
    iobuf_read_8(reader, &b);

    if (b > VFD_BRIGHTNESS_MAX){
        dprintf("VFD: Brightness, invalid argument\n");
        return E_FAIL;
    }

    dprintf("VFD: Brightness, %d\n", b);
    vfd_state.brightness = b;
    vfd_publish_state();

    return S_FALSE;
}
HRESULT vfd_handle_set_screen_on(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    uint8_t b;
    iobuf_read_8(reader, &b);

    if (b > 1){
        dprintf("VFD: Screen Power, invalid argument\n");
        return E_FAIL;
    }

    dprintf("VFD: Screen Power, %d\n", b);
    vfd_state.screen_on = b;
    vfd_publish_state();
    return S_FALSE;
}
HRESULT vfd_handle_set_h_scroll(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    uint16_t x;
    iobuf_read_be16(reader, &x);

    dprintf("VFD: Horizontal Scroll, X=%d\n", x);
    vfd_state.h_scroll = x;
    vfd_publish_state();
    return S_FALSE;
}
HRESULT vfd_handle_draw_image(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    uint16_t x0;
    uint16_t w;
    uint16_t h_lines;
    uint16_t h_pixels;
    uint8_t y0, y1;
    size_t payload;
    size_t remaining;

    iobuf_read_be16(reader, &x0);
    iobuf_read_8(reader, &y0);
    iobuf_read_be16(reader, &w);
    iobuf_read_8(reader, &y1);

    if (y1 >= y0) {
        h_lines = (uint16_t) (y1 - y0 + 1);
    } else {
        h_lines = 0;
    }

    h_pixels = (uint16_t) (h_lines * 8);
    dprintf("VFD: Draw image, %dx%d @%d,%d\n", w, h_pixels, x0, y0);

    payload = (size_t) w * (size_t) h_pixels;
    remaining = reader->nbytes - reader->pos;
    if (payload > remaining) {
        payload = remaining;
    }

    reader->pos += payload;
    return S_FALSE;
}

HRESULT vfd_handle_set_cursor(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    uint16_t x;
    uint8_t y;

    iobuf_read_be16(reader, &x);
    iobuf_read_8(reader, &y);

    dprintf("VFD: Set Cursor, x=%d,y=%d\n", x, y);
    vfd_state.cursor_x = x;
    vfd_state.cursor_y = y;
    vfd_publish_state();

    return S_FALSE;
}

HRESULT vfd_handle_set_encoding(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    uint8_t b;
    iobuf_read_8(reader, &b);

    dprintf("VFD: Set Encoding, %d (%s)\n", b, get_encoding_name(b));

    if (b < 0 || b > VFD_ENC_MAX){
        dprintf("Invalid encoding specified\n");
        return E_FAIL;
    }

    vfd_state.encoding = b;
    vfd_publish_state();

    return S_FALSE;
}
HRESULT vfd_handle_set_text_wnd(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    uint16_t x0, w;
    uint8_t y0, h;
    uint16_t x1;
    uint8_t y1;

    iobuf_read_be16(reader, &x0);
    iobuf_read_8(reader, &y0);
    iobuf_read_be16(reader, &w);
    iobuf_read_8(reader, &h);

    x1 = (uint16_t) (x0 + w);
    y1 = (uint8_t) (y0 + h);

    dprintf("VFD: Set Text Window, x=%d,y=%d,w=%d,h=%d\n", x0, y0, w, h);
    vfd_state.wnd_x0 = x0;
    vfd_state.wnd_y0 = y0;
    vfd_state.wnd_x1 = x1;
    vfd_state.wnd_y1 = y1;
    vfd_publish_state();
    return S_FALSE;
}
HRESULT vfd_handle_set_text_speed(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    uint8_t b;
    iobuf_read_8(reader, &b);

    dprintf("VFD: Set Text Speed, %d\n", b);
    vfd_state.text_speed = b;
    vfd_publish_state();
    return S_FALSE;
}
HRESULT vfd_handle_write_text(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    uint8_t len;
    iobuf_read_8(reader, &len);

    char* str = malloc((size_t) len + 1);
    iobuf_read(reader, str, len);
    str[len] = '\0';

    print_vfd_text(str, len);
    free(str);

    return S_FALSE;
}
HRESULT vfd_handle_enable_scroll(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    dprintf("VFD: Enable Scrolling\n");
    vfd_state.scroll_enabled = 1;
    vfd_publish_state();
    return S_FALSE;
}
HRESULT vfd_handle_disable_scroll(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    dprintf("VFD: Disable Scrolling\n");
    vfd_state.scroll_enabled = 0;
    vfd_publish_state();
    return S_FALSE;
}
HRESULT vfd_handle_rotate(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    uint8_t b;
    iobuf_read_8(reader, &b);

    dprintf("VFD: Rotate, %d\n", b);
    vfd_state.rotate = b;
    vfd_publish_state();
    return S_FALSE;
}
HRESULT vfd_handle_create_char(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    uint8_t b;
    iobuf_read_8(reader, &b);
    char buf[32];

    iobuf_read(reader, buf, 32);

    dprintf("VFD: Create character, %d\n", b);
    return S_FALSE;
}
HRESULT vfd_handle_create_char2(struct const_iobuf* reader, struct iobuf* writer, struct uart* vfd_uart){
    uint8_t b, b2;
    iobuf_read_8(reader, &b);
    iobuf_read_8(reader, &b2);
    char buf[16];

    iobuf_read(reader, buf, 16);

    dprintf("VFD: Create character, %d, %d\n", b, b2);
    return S_FALSE;
}
