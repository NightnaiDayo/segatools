#include "backend.h"

static void diva_mouse_update(diva_io_touch_callback_t callback);

static const struct diva_touch_backend diva_mouse_backend = {
    .update = diva_mouse_update
};

static int diva_io_m1 = VK_LBUTTON;

HRESULT diva_touch_mouse_init(const struct diva_touch_backend** backend) {
    diva_io_m1 = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;

    *backend = &diva_mouse_backend;

    return S_OK;
}

static uint8_t status = 0;
static uint16_t x = 0;
static uint16_t y = 0;
static uint16_t last_x = 0;
static uint16_t last_y = 0;
static uint8_t id = 0;
static bool touch = false;

void diva_mouse_update(diva_io_touch_callback_t callback) {
    if (GetAsyncKeyState(diva_io_m1) & 0x8000) {
        POINT point;

        /* Get cursor location and map to window size */
        BOOL ok = GetCursorPos(&point);

        if (!ok) {
            return;
        }

        HWND hwnd = GetForegroundWindow();

        ok = ScreenToClient(hwnd, &point);

        if (!ok) {
            return;
        }

        /* Set status */
        if (!touch) {
            status = DIVA_IO_TOUCH_DOWN;
            touch = true;
        } else {
            status = DIVA_IO_TOUCH_STREAM;
        }

        /* Set coordinates */
        if (point.x < 0) point.x = 0;
        if (point.y < 0) point.y = 0;
        x = (uint16_t)point.x;
        y = (uint16_t)point.y;
        last_x = x;
        last_y = y;
    } else {
        if (touch) {
            status = DIVA_IO_TOUCH_LIFTOFF;
            x = last_x;
            y = last_y;
            touch = false;
        } else {
            /* No touch event */
            status = 0;
            x = 0;
            y = 0;
        }
    }

    /* Mouse always acts as single-touch */
    id = 1;

    callback(status, x, y, id);
}
