#pragma once

#include <windows.h>

#include <stdint.h>

enum {
    SEKITO_IO_OPBTN_TEST = 0x01,
    SEKITO_IO_OPBTN_SERVICE = 0x02,
    SEKITO_IO_OPBTN_COIN = 0x04,
    SEKITO_IO_OPBTN_SW1 = 0x08,
    SEKITO_IO_OPBTN_SW2 = 0x10,
};

enum {
    SEKITO_IO_GAMEBTN_MENU = 0x01,
    SEKITO_IO_GAMEBTN_START = 0x02,
    SEKITO_IO_GAMEBTN_STRATAGEM = 0x04,
    SEKITO_IO_GAMEBTN_STRATAGEM_LOCK = 0x08,
    SEKITO_IO_GAMEBTN_HOUGU = 0x10,
    SEKITO_IO_GAMEBTN_NUMPAD_0 = 0x100,
    SEKITO_IO_GAMEBTN_NUMPAD_1 = 0x200,
    SEKITO_IO_GAMEBTN_NUMPAD_2 = 0x400,
    SEKITO_IO_GAMEBTN_NUMPAD_3 = 0x800,
    SEKITO_IO_GAMEBTN_NUMPAD_4 = 0x1000,
    SEKITO_IO_GAMEBTN_NUMPAD_5 = 0x2000,
    SEKITO_IO_GAMEBTN_NUMPAD_6 = 0x4000,
    SEKITO_IO_GAMEBTN_NUMPAD_7 = 0x8000,
    SEKITO_IO_GAMEBTN_NUMPAD_8 = 0x10000,
    SEKITO_IO_GAMEBTN_NUMPAD_9 = 0x20000,
    SEKITO_IO_GAMEBTN_NUMPAD_CLEAR = 0x40000,
    SEKITO_IO_GAMEBTN_NUMPAD_ENTER = 0x80000,
    SEKITO_IO_GAMEBTN_TERMINAL_LEFT = 0x400000,
    SEKITO_IO_GAMEBTN_TERMINAL_UP = 0x800000,
    SEKITO_IO_GAMEBTN_TERMINAL_RIGHT = 0x1000000,
    SEKITO_IO_GAMEBTN_TERMINAL_DOWN = 0x2000000,
    SEKITO_IO_GAMEBTN_TERMINAL_LEFT_2 = 0x4000000,
    SEKITO_IO_GAMEBTN_TERMINAL_RIGHT_2 = 0x8000000,
    SEKITO_IO_GAMEBTN_TERMINAL_DECIDE = 0x10000000,
    SEKITO_IO_GAMEBTN_TERMINAL_CANCEL = 0x20000000,
    SEKITO_IO_GAMEBTN_TERMINAL_RESERVE = 0x40000000,
};

/* Get the version of the Eiketsu Taisen IO API that this DLL supports. This
   function should return a positive 16-bit integer, where the high byte is
   the major version and the low byte is the minor version (as defined by the
   Semantic Versioning standard).

   The latest API version as of this writing is 0x0100. */

uint16_t sekito_io_get_api_version(void);

/* Initialize the IO DLL. This is the second function that will be called on
   your DLL, after sekito_io_get_api_version.

   All subsequent calls to this API may originate from arbitrary threads.

   Minimum API version: 0x0100 */

HRESULT sekito_io_init(void);

/* Send any queued outputs (of which there are currently none, though this may
   change in subsequent API versions) and retrieve any new inputs.

   Minimum API version: 0x0100 */

HRESULT sekito_io_poll(void);

/* Get the state of the cabinet's operator buttons as of the last poll. See
   SEKITO_IO_OPBTN enum above: this contains bit mask definitions for button
   states returned in *opbtn. All buttons are active-high.

   Minimum API version: 0x0100 */

void sekito_io_get_opbtns(uint8_t *opbtn);

/* Get the state of the cabinet's gameplay buttons as of the last poll. See
   SEKITO_IO_GAMEBTN enum above: this contains bit mask definitions for button
   states returned in *gamebtn. All buttons are active-high.

   Minimum API version: 0x0100 */

void sekito_io_get_gamebtns(uint32_t *gamebtn);

/* Get the position of the trackball as of the last poll.

   Minimum API version: 0x0100 */

void sekito_io_get_trackball_position(uint16_t *stick_x, uint16_t *stick_y);

/* Initialize LED emulation. This function will be called before any
   other sekito_io_led_*() function calls.

   All subsequent calls may originate from arbitrary threads and some may
   overlap with each other. Ensuring synchronization inside your IO DLL is
   your responsibility. */

HRESULT sekito_io_led_init(void);

/* Update the RGB LEDs. 
   
   Exact layout is TBD. */

void sekito_io_led_set_colors(uint8_t board, uint8_t *rgb);
