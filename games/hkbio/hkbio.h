#pragma once

#include <windows.h>

#include <stdint.h>

enum {
    HKB_IO_OPBTN_TEST = 0x01,
    HKB_IO_OPBTN_SERVICE = 0x02,
    HKB_IO_OPBTN_COIN = 0x04,
};

enum {
    HKB_IO_GAMEBTN_TERMINAL_LEFT = 0x01,
    HKB_IO_GAMEBTN_TERMINAL_UP = 0x02,
    HKB_IO_GAMEBTN_TERMINAL_RIGHT = 0x04,
    HKB_IO_GAMEBTN_TERMINAL_DOWN = 0x08,
    HKB_IO_GAMEBTN_TERMINAL_LEFT_2 = 0x10,
    HKB_IO_GAMEBTN_TERMINAL_RIGHT_2 = 0x20,
    HKB_IO_GAMEBTN_TERMINAL_DECIDE = 0x40,
    HKB_IO_GAMEBTN_TERMINAL_CANCEL = 0x80,
};

/* Get the version of the Chrono Regalia IO API that this DLL supports. This
   function should return a positive 16-bit integer, where the high byte is
   the major version and the low byte is the minor version (as defined by the
   Semantic Versioning standard).

   The latest API version as of this writing is 0x0100. */

uint16_t hkb_io_get_api_version(void);

/* Initialize the IO DLL. This is the second function that will be called on
   your DLL, after hkb_io_get_api_version.

   All subsequent calls to this API may originate from arbitrary threads.

   Minimum API version: 0x0100 */

HRESULT hkb_io_init(void);

/* Send any queued outputs (of which there are currently none, though this may
   change in subsequent API versions) and retrieve any new inputs.

   Minimum API version: 0x0100 */

HRESULT hkb_io_poll(void);

/* Get the state of the cabinet's operator buttons as of the last poll. See
   HKB_IO_OPBTN enum above: this contains bit mask definitions for button
   states returned in *opbtn. All buttons are active-high.

   Minimum API version: 0x0100 */

void hkb_io_get_opbtns(uint8_t* opbtn);

/* Get the state of the cabinet's gameplay buttons as of the last poll. See
   HKB_IO_GAMEBTN enum above: this contains bit mask definitions for button
   states returned in *gamebtn. All buttons are active-high.

   Minimum API version: 0x0100 */

void hkb_io_get_gamebtns(uint16_t* gamebtn);

/* Get the position of the cabinet stick as of the last poll. The center
   position should be equal to or close to 32767.

   Minimum API version: 0x0100 */

void hkb_io_get_analogs(int16_t* stick_x, int16_t* stick_y);

/* Initialize LED emulation. This function will be called before any
   other hkb_io_led_*() function calls.

   All subsequent calls may originate from arbitrary threads and some may
   overlap with each other. Ensuring synchronization inside your IO DLL is
   your responsibility. */

HRESULT hkb_io_led_init(void);

/* Update the RGB LEDs. 
   
   Exact layout is TBD. */

void hkb_io_led_set_colors(uint8_t board, uint8_t* rgb);