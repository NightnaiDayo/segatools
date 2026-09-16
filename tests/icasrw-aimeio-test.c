/* Standalone Windows shared-backend tests with fail-fast UART stubs.
 * zig cc -target x86_64-windows-gnu -DNDEBUG -Icommon -I<capnhook>
 *     tests/icasrw-aimeio-test.c -o icasrw-aimeio-test.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include "board/sg-reader.c"
struct aime_dll aime_dll;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static unsigned int polls;
static HRESULT poll_result = S_OK;
static HRESULT get_result = S_OK;
static HRESULT radio_result = S_OK;
static uint64_t card_id = 0x036f9e9c732c34b6ULL;
static HRESULT poll(uint8_t unit) { CHECK(unit == 0); polls++; return poll_result; }
static HRESULT get(uint8_t unit, uint64_t *idm) { CHECK(unit == 0); *idm = card_id; return get_result; }
static HRESULT radio(uint8_t unit) { CHECK(unit == 0); return radio_result; }
static HRESULT get_access(uint8_t unit, uint8_t *code, size_t size)
{
    CHECK(unit == 0 && size == 10);
    memset(code, 0x12, 10);
    return get_result;
}
int main(void)
{
    uint64_t idm = 1;
    CHECK(segatools_icas_aimeio_poll(NULL) == E_POINTER);
    CHECK(segatools_icas_aimeio_poll(&idm) == E_PENDING && idm == 0);
    InitializeCriticalSection(&sg_reader_lock);
    InterlockedExchange(&sg_reader_ready, 1);
    CHECK(segatools_icas_aimeio_poll(&idm) == E_PENDING);
    aime_dll.nfc_poll = poll;
    aime_dll.nfc_get_felica_id = get;
    sg_reader_started = true;
    sg_reader_start_hr = E_FAIL;
    CHECK(segatools_icas_aimeio_poll(&idm) == E_FAIL && polls == 0);
    sg_reader_start_hr = S_OK;
    CHECK(segatools_icas_aimeio_poll(&idm) == S_OK && idm == card_id);
    get_result = S_FALSE;
    CHECK(segatools_icas_aimeio_poll(&idm) == S_FALSE);
    get_result = S_OK;
    card_id = 0;
    CHECK(segatools_icas_aimeio_poll(&idm) == S_FALSE);
    poll_result = E_FAIL;
    CHECK(segatools_icas_aimeio_poll(&idm) == E_FAIL);
    poll_result = S_OK;
    card_id = 1;
    aime_dll.nfc_radio_on = radio;
    radio_result = E_NOTIMPL;
    CHECK(segatools_icas_aimeio_poll(&idm) == S_OK);
    radio_result = E_FAIL;
    CHECK(segatools_icas_aimeio_poll(&idm) == E_FAIL);
    radio_result = S_OK;
    aime_dll.nfc_get_aime_id = get_access;
    uint8_t code[10];
    CHECK(segatools_icas_aimeio_poll_access_code(NULL) == E_POINTER);
    CHECK(segatools_icas_aimeio_poll_access_code(code) == S_OK);
    CHECK(code[0] == 0x12 && code[9] == 0x12);
    get_result = S_FALSE;
    CHECK(segatools_icas_aimeio_poll_access_code(code) == S_FALSE);
    get_result = E_FAIL;
    CHECK(segatools_icas_aimeio_poll_access_code(code) == E_FAIL);
    DeleteCriticalSection(&sg_reader_lock);
    puts("ICAS shared AimeIO tests passed");
    return 0;
}
/* Unused UART dependencies fail loudly if the payment bridge reaches them. */
HRESULT aime_dll_init(const struct aime_dll_config *c, HINSTANCE h) { abort(); }
void sg_nfc_init(struct sg_nfc *n, uint8_t a, const struct sg_nfc_ops *o, unsigned int g, unsigned int p, const wchar_t *f, bool m, void *c) { abort(); }
void sg_led_init(struct sg_led *l, uint8_t a, const struct sg_led_ops *o, unsigned int g, void *c) { abort(); }
void uart_init(struct uart *u, unsigned int p) { abort(); }
HRESULT iohook_push_handler(iohook_fn_t f) { abort(); }
bool uart_match_irp(const struct uart *u, const struct irp *i) { abort(); }
HRESULT iohook_invoke_next(struct irp *i) { abort(); }
HRESULT uart_handle_irp(struct uart *u, struct irp *i) { abort(); }
void sg_nfc_transact(struct sg_nfc *n, struct iobuf *b, const void *r, size_t s) { abort(); }
void sg_led_transact(struct sg_led *l, struct iobuf *b, const void *r, size_t s) { abort(); }
