#pragma once

#include <windows.h>

#include <stdint.h>

enum {
    SOUL_IO_OPBTN_TEST = 0x01,
    SOUL_IO_OPBTN_SERVICE = 0x02,
    SOUL_IO_OPBTN_COIN = 0x04,
};

enum {
    SOUL_IO_GAMEBTN_A = 0x01,
    SOUL_IO_GAMEBTN_B = 0x02,
    SOUL_IO_GAMEBTN_C = 0x04,
};

/* Get the version of the Soul Reverse IO API that this DLL supports. This
   function should return a positive 16-bit integer, where the high byte is
   the major version and the low byte is the minor version (as defined by the
   Semantic Versioning standard).

   The latest API version as of this writing is 0x0100. */

uint16_t soul_io_get_api_version(void);

/* Initialize the IO DLL. This is the second function that will be called on
   your DLL, after soul_io_get_api_version.

   All subsequent calls to this API may originate from arbitrary threads.

   Minimum API version: 0x0100 */

HRESULT soul_io_init(void);

/* Send any queued outputs (of which there are currently none, though this may
   change in subsequent API versions) and retrieve any new inputs.

   Minimum API version: 0x0100 */

HRESULT soul_io_poll(void);

/* Get the state of the cabinet's operator buttons as of the last poll. See
   SOUL_IO_OPBTN enum above: this contains bit mask definitions for button
   states returned in *opbtn. All buttons are active-high.

   Minimum API version: 0x0100 */

void soul_io_get_opbtns(uint8_t *opbtn);

/* Get the state of the cabinet's gameplay buttons as of the last poll. See
   SOUL_IO_GAMEBTN enum above for bit mask definitions. Inputs are split into
   a left hand side set of inputs and a right hand side set of inputs: the bit
   mappings are the same in both cases.

   All buttons are active-high, even though some buttons' electrical signals
   on a real cabinet are active-low.

   Minimum API version: 0x0100 */

void soul_io_get_gamebtns(uint8_t *gamebtn);

/* Initialize LED emulation. This function will be called before any
   other soul_io_led_*() function calls.

   All subsequent calls may originate from arbitrary threads and some may
   overlap with each other. Ensuring synchronization inside your IO DLL is
   your responsibility. */

HRESULT soul_io_led_init(void);

/* Update the RGB LEDs. 
   
   Exact layout is TBD. */

void soul_io_led_set_colors(uint8_t board, uint8_t *rgb);
