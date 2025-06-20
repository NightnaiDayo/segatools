#pragma once

#include <windows.h>

#include <stdint.h>

enum {
    KEMONO_IO_OPBTN_TEST = 0x01,
    KEMONO_IO_OPBTN_SERVICE = 0x02
};

enum {
    KEMONO_IO_GAMEBTN_UP = 0x01,
    KEMONO_IO_GAMEBTN_DOWN = 0x02,
    KEMONO_IO_GAMEBTN_LEFT = 0x04,
    KEMONO_IO_GAMEBTN_RIGHT = 0x08,
    KEMONO_IO_GAMEBTN_R = 0x10,
    KEMONO_IO_GAMEBTN_G = 0x20,
    KEMONO_IO_GAMEBTN_B = 0x40,
    KEMONO_IO_GAMEBTN_START = 0x80
};

/* Get the version of the Kemono IO API that this DLL supports. This
   function should return a positive 16-bit integer, where the high byte is
   the major version and the low byte is the minor version (as defined by the
   Semantic Versioning standard).

   The latest API version as of this writing is 0x0100. */

uint16_t kemono_io_get_api_version(void);

/* Initialize the IO DLL. This is the second function that will be called on
   your DLL, after kemono_io_get_api_version.

   All subsequent calls to this API may originate from arbitrary threads.

   Minimum API version: 0x0100 */

HRESULT kemono_io_init(void);

/* Send any queued outputs (of which there are currently none, though this may
   change in subsequent API versions) and retrieve any new inputs.

   Minimum API version: 0x0100 */

HRESULT kemono_io_poll(uint16_t* ops, uint16_t* player);

/* Read the current state of the coin counter. This value should be incremented
   for every coin detected by the coin acceptor mechanism. This count does not
   need to persist beyond the lifetime of the process.

   Minimum API version: 0x0100 */
void kemono_io_jvs_read_coin_counter(uint16_t *out);

/* Initialize LED emulation. This function will be called before any
   other fgo_io_led_*() function calls.

   All subsequent calls may originate from arbitrary threads and some may
   overlap with each other. Ensuring synchronization inside your IO DLL is
   your responsibility.

   Minimum API version: 0x0100 */
HRESULT kemono_io_led_init(void);

/* Update the RGB LEDs.

    The left side LED bar are indices 0 to 32.
    The right side LED bar are indices 33 to 65.

    Minimum API version: 0x0100 */
void kemono_io_led_set_colors(uint8_t board, uint8_t *rgb);

/* Update the button LEDs.

    Button R: Bit 15
    Button G: Bit 1
    Button B: Bit 13
    Start Button: Bit 11

    Minimum API version: 0x0100 */
void kemono_io_jvs_write_gpio(uint32_t state);