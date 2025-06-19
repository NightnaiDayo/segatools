#pragma once

#include <windows.h>

#include <stdint.h>

enum {
    TOKYO_IO_OPBTN_TEST = 0x01,
    TOKYO_IO_OPBTN_SERVICE = 0x02,
    TOKYO_IO_OPBTN_COIN = 0x04,
};

enum {
    TOKYO_IO_GAMEBTN_BLUE = 0x01,
    TOKYO_IO_GAMEBTN_YELLOW = 0x02,
    TOKYO_IO_GAMEBTN_RED = 0x04,
};

enum {
    TOKYO_IO_SENSE_FOOT_LEFT = 0x01,
    TOKYO_IO_SENSE_FOOT_RIGHT = 0x02,
    TOKYO_IO_SENSE_JUMP_1 = 0x04,
    TOKYO_IO_SENSE_JUMP_2 = 0x08,
    TOKYO_IO_SENSE_JUMP_3 = 0x10,
    TOKYO_IO_SENSE_JUMP_4 = 0x20,
    TOKYO_IO_SENSE_JUMP_5 = 0x40,
    TOKYO_IO_SENSE_JUMP_6 = 0x80,
};

enum {
    /* These are the bitmasks to use when checking which
       lights are triggered on incoming IO4 GPIO writes. */
    TOKYO_IO_LED_LEFT_BLUE = 1 << 31,
    TOKYO_IO_LED_CENTER_YELLOW = 1 << 30,
    TOKYO_IO_LED_RIGHT_RED = 1 << 29,
    TOKYO_IO_LED_CONTROL_LEFT_R = 1 << 25,
    TOKYO_IO_LED_CONTROL_LEFT_G = 1 << 24,
    TOKYO_IO_LED_CONTROL_LEFT_B = 1 << 23,
    TOKYO_IO_LED_CONTROL_RIGHT_R = 1 << 22,
    TOKYO_IO_LED_CONTROL_RIGHT_G = 1 << 21,
    TOKYO_IO_LED_CONTROL_RIGHT_B = 1 << 20,
    TOKYO_IO_LED_FLOOR_LEFT_R = 1 << 19,
    TOKYO_IO_LED_FLOOR_LEFT_G = 1 << 18,
    TOKYO_IO_LED_FLOOR_LEFT_B = 1 << 17,
    TOKYO_IO_LED_FLOOR_RIGHT_R = 1 << 16,
    TOKYO_IO_LED_FLOOR_RIGHT_G = 1 << 15,
    TOKYO_IO_LED_FLOOR_RIGHT_B = 1 << 14,
};

/* Get the version of the Mario & Sonic at the Olympic Games Tokyo 2020 Arcade
   Edition IO API that this DLL supports. This function should return a
   positive 16-bit integer, where the high byte is the major version and the
   low byte is the minor version (as defined by the Semantic Versioning
   standard).

   The latest API version as of this writing is 0x0100. */

uint16_t tokyo_io_get_api_version(void);

/* Initialize the IO DLL. This is the second function that will be called on
   your DLL, after tokyo_io_get_api_version.

   All subsequent calls to this API may originate from arbitrary threads.

   Minimum API version: 0x0100 */

HRESULT tokyo_io_init(void);

/* Send any queued outputs (of which there are currently none, though this may
   change in subsequent API versions) and retrieve any new inputs.

   Minimum API version: 0x0100 */

HRESULT tokyo_io_poll(void);

/* Get the state of the cabinet's operator buttons as of the last poll. See
   TOKYO_IO_OPBTN enum above: this contains bit mask definitions for button
   states returned in *opbtn. All buttons are active-high.

   Minimum API version: 0x0100 */

void tokyo_io_get_opbtns(uint8_t *opbtn);

/* Get the state of the cabinet's gameplay buttons as of the last poll. See
   TOKYO_IO_GAMEBTN enum above: this contains bit mask definitions for button
   states returned in *gamebtn. All buttons are active-high.

   Minimum API version: 0x0100 */

void tokyo_io_get_gamebtns(uint8_t *gamebtn);

/* Get the state of the cabinet's gameplay buttons as of the last poll. See
   TOKYO_IO_SENSE enum above: this contains bit mask definitions for button
   states returned in *sense. All buttons are active-high.

   Minimum API version: 0x0100 */

void tokyo_io_get_sensors(uint8_t *sense);

/* Initialize LED emulation. This function will be called before any
   other tokyo_io_led_*() function calls.

   All subsequent calls may originate from arbitrary threads and some may
   overlap with each other. Ensuring synchronization inside your IO DLL is
   your responsibility. 
   
   Minimum API version: 0x0100 */

HRESULT tokyo_io_led_init(void);

/* Update the RGB LEDs. rgb is a pointer to an array of up to 54 * 3 = 162 bytes.

   Mario & Sonic at the Tokyo 2020 Olympics Arcade uses one board with 15 LEDs for
   all buttons, control panel, and floor LEDs. Board 1 is just used for the
   left and right monitor LEDs.

   Board 0 has 54 LEDs (GRB order):
      [0]-[26]: left monitor LEDs
      [27]-[53]: right monitor LEDs
   
   Board 1 has 15 LEDs (RGB order):
      [0]: left blue LED
      [1]: center yellow LED
      [2]: right red LED
      [3]-[5]: left control panel LEDs
      [6]-[8]: right control panel LEDs
      [9]-[11]: left floor LEDs
      [12]-[14]: right floor LEDs
   
   Each rgb value is comprised of 3 bytes in G,R,B order for board 0 and R,G,B 
   order for board 1. The tricky part is that the board 0 is called from app and 
   the board 1 is called from amdaemon. So the library must be able to handle both 
   calls, using shared memory f.e. This is up to the developer to decide how to
   handle this, recommended way is to use the amdaemon process as the main one 
   and the app process as a sub one.

   Minimum API version: 0x0100 */

void tokyo_io_led_set_colors(uint8_t board, uint8_t *rgb);
