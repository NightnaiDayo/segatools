#pragma once

/*
   MU3 CUSTOM IO API

   Changelog:

   - 0x0100: Initial API version (assumed if chuni_io_get_api_version is not
     exported)
   - 0x0101: Added mu3_io_led_init and mu3_io_set_leds
*/

#include <windows.h>

#include <stdint.h>

enum {
    MU3_IO_OPBTN_TEST = 0x01,
    MU3_IO_OPBTN_SERVICE = 0x02,
    MU3_IO_OPBTN_COIN = 0x04,
};

enum {
    MU3_IO_GAMEBTN_1 = 0x01,
    MU3_IO_GAMEBTN_2 = 0x02,
    MU3_IO_GAMEBTN_3 = 0x04,
    MU3_IO_GAMEBTN_SIDE = 0x08,
    MU3_IO_GAMEBTN_MENU = 0x10,
};

enum {
    /* These are the bitmasks to use when checking which
       lights are triggered on incoming IO4 GPIO writes. */
    MU3_IO_LED_L1_R = 1 << 31,
    MU3_IO_LED_L1_G = 1 << 28,
    MU3_IO_LED_L1_B = 1 << 30,
    MU3_IO_LED_L2_R = 1 << 27,
    MU3_IO_LED_L2_G = 1 << 29,
    MU3_IO_LED_L2_B = 1 << 26,
    MU3_IO_LED_L3_R = 1 << 25,
    MU3_IO_LED_L3_G = 1 << 24,
    MU3_IO_LED_L3_B = 1 << 23,
    MU3_IO_LED_R1_R = 1 << 22,
    MU3_IO_LED_R1_G = 1 << 21,
    MU3_IO_LED_R1_B = 1 << 20,
    MU3_IO_LED_R2_R = 1 << 19,
    MU3_IO_LED_R2_G = 1 << 18,
    MU3_IO_LED_R2_B = 1 << 17,
    MU3_IO_LED_R3_R = 1 << 16,
    MU3_IO_LED_R3_G = 1 << 15,
    MU3_IO_LED_R3_B = 1 << 14,
};

/* Get the version of the Ongeki IO API that this DLL supports. This
   function should return a positive 16-bit integer, where the high byte is
   the major version and the low byte is the minor version (as defined by the
   Semantic Versioning standard).

   The latest API version as of this writing is 0x0100. */

uint16_t mu3_io_get_api_version(void);

/* Initialize the IO DLL. This is the second function that will be called on
   your DLL, after mu3_io_get_api_version.

   All subsequent calls to this API may originate from arbitrary threads.

   Minimum API version: 0x0100 */

HRESULT mu3_io_init(void);

/* Send any queued outputs (of which there are currently none, though this may
   change in subsequent API versions) and retrieve any new inputs.

   Minimum API version: 0x0100 */

HRESULT mu3_io_poll(void);

/* Get the state of the cabinet's operator buttons as of the last poll. See
   MU3_IO_OPBTN enum above: this contains bit mask definitions for button
   states returned in *opbtn. All buttons are active-high.

   Minimum API version: 0x0100 */

void mu3_io_get_opbtns(uint8_t *opbtn);

/* Get the state of the cabinet's gameplay buttons as of the last poll. See
   MU3_IO_GAMEBTN enum above for bit mask definitions. Inputs are split into
   a left hand side set of inputs and a right hand side set of inputs: the bit
   mappings are the same in both cases.

   All buttons are active-high, even though some buttons' electrical signals
   on a real cabinet are active-low.

   Minimum API version: 0x0100 */

void mu3_io_get_gamebtns(uint8_t *left, uint8_t *right);

/* Get the position of the cabinet lever as of the last poll. The center
   position should be equal to or close to zero.

   The operator will be required to calibrate the lever's range of motion on
   first power-on, so the lever position reported through this API does not
   need to perfectly centered or cover every single position value possible,
   but it should be reasonably close in order to make things easier for the
   operator.

   The calibration screen displays the leftmost and rightmost position signal
   returned from the cabinet's ADC encoder as a pair of raw two's complement
   hexadecimal values. On a real cabinet these leftmost and rightmost
   positions are somewhere around 0xB000 and 0x5000 respectively (remember
   that negative values i.e. left positions have a high most-significant bit),
   although these values can easily vary by +/- 0x1000 across different
   cabinets.

   Minimum API version: 0x0100 */

void mu3_io_get_lever(int16_t *pos);

/* Initialize LED emulation. This function will be called before any
   other mu3_io_led_*() function calls.

   All subsequent calls may originate from arbitrary threads and some may
   overlap with each other. Ensuring synchronization inside your IO DLL is
   your responsibility. 
   
   Minimum API version: 0x0101 */

HRESULT mu3_io_led_init(void);

/* Update the RGB LEDs. rgb is a pointer to an array of up to 61 * 3 = 183 bytes.

   ONGEKI uses one board with WS2811 protocol (each logical led corresponds to 3 
   physical leds). Board 0 is used for all cab lights and both WAD button lights.

   Board 0 has 61 LEDs:
      [0]-[1]: left side button
      [2]-[8]: left pillar lower LEDs
      [9]-[17]: left pillar center LEDs
      [18]-[24]: left pillar upper LEDs
      [25]-[35]: billboard LEDs
      [36]-[42]: right pillar upper LEDs
      [43]-[51]: right pillar center LEDs
      [52]-[58]: right pillar lower LEDs
      [59]-[60]: right side button

   Board 1 has 6 LEDs:
      [0]-[5]: 3 left and 3 right controller buttons

   Each rgb value is comprised of 3 bytes in R,G,B order. The tricky part is
   that the board 0 is called from mu3 and the board 1 is called from amdaemon.
   So the library must be able to handle both calls, using shared memory f.e.
   This is up to the developer to decide how to handle this, recommended way is
   to use the amdaemon process as the main one and the mu3 process as a sub one.

   Minimum API version: 0x0101 */

void mu3_io_led_set_colors(uint8_t board, uint8_t *rgb);
