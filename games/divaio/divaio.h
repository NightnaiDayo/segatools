#pragma once

/*
   DIVA CUSTOM IO API

   Changelog:

   - 0x0100: Initial API version
   - 0x0101: Add partition and button led support
*/

#include <windows.h>

#include <stdbool.h>
#include <stdint.h>

enum {
    DIVA_IO_OPBTN_TEST     = 0x01,
    DIVA_IO_OPBTN_SERVICE  = 0x02
};

enum {
    DIVA_IO_GAMEBTN_CIRCLE    = 0x01,
    DIVA_IO_GAMEBTN_CROSS     = 0x02,
    DIVA_IO_GAMEBTN_SQUARE    = 0x04,
    DIVA_IO_GAMEBTN_TRIANGLE  = 0x08,
    DIVA_IO_GAMEBTN_START     = 0x10,
};

enum {
    /* These are the bitmasks to use when checking which
       lights are triggered on incoming IO3 GPIO writes. */
    DIVA_IO_LED_LEFT_PARTITION_RED    = 1 << 1, 
    DIVA_IO_LED_LEFT_PARTITION_GREEN  = 1 << 0, 
    DIVA_IO_LED_LEFT_PARTITION_BLUE   = 1 << 15, 
    DIVA_IO_LED_RIGHT_PARTITION_RED   = 1 << 14, 
    DIVA_IO_LED_RIGHT_PARTITION_GREEN = 1 << 13, 
    DIVA_IO_LED_RIGHT_PARTITION_BLUE  = 1 << 12, 
    DIVA_IO_LED_BTN_TRIANGLE          = 1 << 6, 
    DIVA_IO_LED_BTN_CROSS             = 1 << 3, 
    DIVA_IO_LED_BTN_SQUARE            = 1 << 5,
    DIVA_IO_LED_BTN_CIRCLE            = 1 << 2 
};

/* Get the version of the Project Diva IO API that this DLL supports. This
   function should return a positive 16-bit integer, where the high byte is
   the major version and the low byte is the minor version (as defined by the
   Semantic Versioning standard).

   The latest API version as of this writing is 0x0100. */

uint16_t diva_io_get_api_version(void);

/* Initialize JVS-based input. This function will be called before any other
   diva_io_jvs_*() function calls. Errors returned from this function will
   manifest as a disconnected JVS bus.

   All subsequent calls may originate from arbitrary threads and some may
   overlap with each other. Ensuring synchronization inside your IO DLL is
   your responsibility.

   Minimum API version: 0x0100 */

HRESULT diva_io_jvs_init(void);

/* Poll JVS input.

   opbtn returns the cabinet test/service state, where bit 0 is Test and Bit 1
   is Service.

   gamebtn bits, from least significant to most significant, are:

   Circle Cross Square Triangle Start UNUSED UNUSED UNUSED

   Minimum API version: 0x0100 */

void diva_io_jvs_poll(uint8_t *opbtn, uint8_t *gamebtn);

/* Read the current state of the coin counter. This value should be incremented
   for every coin detected by the coin acceptor mechanism. This count does not
   need to persist beyond the lifetime of the process.

   Minimum API Version: 0x0100 */

void diva_io_jvs_read_coin_counter(uint16_t *out);

/* Initialize touch slider emulation. This function will be called before any
   other diva_io_slider_*() function calls.

   All subsequent calls may originate from arbitrary threads and some may
   overlap with each other. Ensuring synchronization inside your IO DLL is
   your responsibility.

   Minimum API version: 0x0100 */

HRESULT diva_io_slider_init(void);

/* Project Diva touch sliders consist of 32 pressure sensitive cells, where
   cell 1 (array index 0) is the rightmost cell and cell 32 (array index 31) is
   the leftmost cell. */

/* Callback function supplied to your IO DLL. This must be called with a
   pointer to a 32-byte array of pressure values, one byte per slider cell.
   Cells reporting a pressure value of at least 20 are considered to be pressed.
   This threshold is not configurable.

   The callback will copy the pressure state data out of your buffer before
   returning. The pointer will not be retained. */

typedef void (*diva_io_slider_callback_t)(const uint8_t *state);

/* Start polling the slider. Your DLL must start a polling thread and call the
   supplied function periodically from that thread with new input state. The
   update interval is up to you, but if your input device doesn't have any
   preferred interval then 1 kHz is a reasonable maximum frequency.

   Note that you do have to have to call the callback "occasionally" even if
   nothing is changing, otherwise the game will raise a comm timeout error.

   Minimum API version: 0x0100 */

void diva_io_slider_start(diva_io_slider_callback_t callback);

/* Stop polling the slider. You must cease to invoke the input callback before
   returning from this function.

   This *will* be called in the course of regular operation. For example,
   every time you go into the operator menu the slider and all of the other I/O
   on the cabinet gets restarted.

   Following on from the above, the slider polling loop *will* be restarted
   after being stopped in the course of regular operation. Do not permanently
   tear down your input driver in response to this function call.

   Minimum API version: 0x0100 */

void diva_io_slider_stop(void);

/* Update the RGB lighting on the slider. A pointer to an array of 32 * 3 = 96
   bytes is supplied. Layout is probably strictly linear but still TBD.

   Minimum API version: 0x0100 */

void diva_io_slider_set_leds(const uint8_t *rgb);

/* Initialize LED emulation. This function will be called before any
   other diva_io_led_*() function calls.

   All subsequent calls may originate from arbitrary threads and some may
   overlap with each other. Ensuring synchronization inside your IO DLL is
   your responsibility. 
   
   Minimum API version: 0x0101 */

HRESULT diva_io_led_init(void);

/* Update the cabinet button LEDs. rgb is a pointer to an array up to 10 bytes.

   The LEDs are laid out as follows:
   [0]: LEFT PARTITION RED LED
   [1]: LEFT PARTITION GREEN LED
   [2]: LEFT PARTITION BLUE LED
   [3]: RIGHT PARTITION RED LED
   [4]: RIGHT PARTITION GREEN LED
   [5]: RIGHT PARTITION BLUE LED
   [6]: BTN TRIANGLE LED
   [7]: BTN CROSS LED
   [8]: BTN SQUARE LED
   [9]: BTN CIRCLE LED

   The LED is turned on when the byte is 255 and turned off when the byte is 0.

   Minimum API version: 0x0101 */

void diva_io_led_set_leds(uint8_t board, const uint8_t *rgb);
