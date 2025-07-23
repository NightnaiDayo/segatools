#pragma once

#include <windows.h>

#include <stdint.h>

enum {
    APM3_IO_OPBTN_TEST = 0x01,
    APM3_IO_OPBTN_SERVICE = 0x02,
    APM3_IO_OPBTN_COIN = 0x04,
};

enum {
    APM3_IO_GAMEBTN_HOME = 0x01,
    APM3_IO_GAMEBTN_START = 0x02,
    APM3_IO_GAMEBTN_UP = 0x04,
    APM3_IO_GAMEBTN_RIGHT = 0x08,
    APM3_IO_GAMEBTN_DOWN = 0x10,
    APM3_IO_GAMEBTN_LEFT = 0x20,
    
    APM3_IO_GAMEBTN_B1 = 0x40,
    APM3_IO_GAMEBTN_B2 = 0x80,
    APM3_IO_GAMEBTN_B3 = 0x100,
    APM3_IO_GAMEBTN_B4 = 0x200,
    APM3_IO_GAMEBTN_B5 = 0x400,
    APM3_IO_GAMEBTN_B6 = 0x800,
    APM3_IO_GAMEBTN_B7 = 0x1000,
    APM3_IO_GAMEBTN_B8 = 0x2000,
};

/* Get the version of the APMv3 IO API that this DLL supports. This
   function should return a positive 16-bit integer, where the high byte is
   the major version and the low byte is the minor version (as defined by the
   Semantic Versioning standard).

   The latest API version as of this writing is 0x0100. */

uint16_t apm3_io_get_api_version(void);

/* Initialize the IO DLL. This is the second function that will be called on
   your DLL, after apm3_io_get_api_version.

   All subsequent calls to this API may originate from arbitrary threads.

   Minimum API version: 0x0100 */

HRESULT apm3_io_init(void);

/* Send any queued outputs (of which there are currently none, though this may
   change in subsequent API versions) and retrieve any new inputs.

   Minimum API version: 0x0100 */

HRESULT apm3_io_poll(void);

/* Get the state of the cabinet's operator buttons as of the last poll. See
   APM3_IO_OPBTN enum above: this contains bit mask definitions for button
   states returned in *opbtn. All buttons are active-high.

   Minimum API version: 0x0100 */

void apm3_io_get_opbtns(uint8_t *opbtn);

/* Get the state of the cabinet's gameplay buttons as of the last poll. See
   APM3_IO_GAMEBTN enum above: this contains bit mask definitions for button
   states returned in *gamebtn. All buttons are active-high.

   Minimum API version: 0x0100 */

void apm3_io_get_gamebtns(uint16_t *gamebtn);

/* Initialize LED emulation. This function will be called before any
   other apm3_io_led_*() function calls.

   All subsequent calls may originate from arbitrary threads and some may
   overlap with each other. Ensuring synchronization inside your IO DLL is
   your responsibility. */

HRESULT apm3_io_led_init(void);

/* Update the RGB LEDs. 
   
   Exact layout is TBD. */

void apm3_io_led_set_colors(uint8_t board, uint8_t *rgb);
