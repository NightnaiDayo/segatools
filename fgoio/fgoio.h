#pragma once

#include <windows.h>

#include <stdint.h>

enum {
    FGO_IO_OPBTN_TEST = 0x01,
    FGO_IO_OPBTN_SERVICE = 0x02,
    FGO_IO_OPBTN_COIN = 0x04,
};

enum {
    FGO_IO_GAMEBTN_SPEED_UP = 0x01,
    FGO_IO_GAMEBTN_TARGET = 0x02,
    FGO_IO_GAMEBTN_ATTACK = 0x04,
    FGO_IO_GAMEBTN_NOBLE_PHANTASM = 0x08,
    FGO_IO_GAMEBTN_CAMERA = 0x10,
};

/* Get the version of the Fate Grand Order IO API that this DLL supports. This
   function should return a positive 16-bit integer, where the high byte is
   the major version and the low byte is the minor version (as defined by the
   Semantic Versioning standard).

   The latest API version as of this writing is 0x0100. */

uint16_t fgo_io_get_api_version(void);

/* Initialize the IO DLL. This is the second function that will be called on
   your DLL, after fgo_io_get_api_version.

   All subsequent calls to this API may originate from arbitrary threads.

   Minimum API version: 0x0100 */

HRESULT fgo_io_init(void);

/* Send any queued outputs (of which there are currently none, though this may
   change in subsequent API versions) and retrieve any new inputs.

   Minimum API version: 0x0100 */

HRESULT fgo_io_poll(void);

/* Get the state of the cabinet's operator buttons as of the last poll. See
   FGO_IO_OPBTN enum above: this contains bit mask definitions for button
   states returned in *opbtn. All buttons are active-high.

   Minimum API version: 0x0100 */

void fgo_io_get_opbtns(uint8_t *opbtn);

/* Get the state of the cabinet's gameplay buttons as of the last poll. See
   FGO_IO_GAMEBTN enum above: this contains bit mask definitions for button
   states returned in *gamebtn. All buttons are active-high.

   Minimum API version: 0x0100 */

void fgo_io_get_gamebtns(uint8_t *gamebtn);

/* Get the position of the cabinet stick as of the last poll. The center
   position should be equal to or close to 32767.

   Minimum API version: 0x0100 */

void fgo_io_get_analogs(int16_t *stick_x, int16_t *stick_y);

/* Initialize LED emulation. This function will be called before any
   other fgo_io_led_*() function calls.

   All subsequent calls may originate from arbitrary threads and some may
   overlap with each other. Ensuring synchronization inside your IO DLL is
   your responsibility. */

HRESULT fgo_io_led_init(void);

/* Update the RGB LEDs. 
   
   Exact layout is TBD. */

void fgo_io_led_set_colors(uint8_t board, uint8_t *rgb);
