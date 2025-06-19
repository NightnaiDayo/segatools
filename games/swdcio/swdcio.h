#pragma once

#include <windows.h>

#include <stdbool.h>
#include <stdint.h>

enum {
    SWDC_IO_OPBTN_TEST = 0x01,
    SWDC_IO_OPBTN_SERVICE = 0x02,
    SWDC_IO_OPBTN_COIN = 0x04,
};

enum {
    SWDC_IO_GAMEBTN_UP = 0x01,
    SWDC_IO_GAMEBTN_DOWN = 0x02,
    SWDC_IO_GAMEBTN_LEFT = 0x04,
    SWDC_IO_GAMEBTN_RIGHT = 0x08,
    SWDC_IO_GAMEBTN_START = 0x10,
    SWDC_IO_GAMEBTN_VIEW_CHANGE = 0x20,

    SWDC_IO_GAMEBTN_STEERING_BLUE = 0x40,
    SWDC_IO_GAMEBTN_STEERING_GREEN = 0x80,
    SWDC_IO_GAMEBTN_STEERING_RED = 0x100,
    SWDC_IO_GAMEBTN_STEERING_YELLOW = 0x200,
    SWDC_IO_GAMEBTN_STEERING_PADDLE_LEFT = 0x400,
    SWDC_IO_GAMEBTN_STEERING_PADDLE_RIGHT = 0x800,
};

enum {
    /* These are the bitmasks to use when checking which
       lights are triggered on incoming IO4 GPIO writes. */
    SWDC_IO_LED_START = 1 << 31,
    SWDC_IO_LED_VIEW_CHANGE = 1 << 30,
    SWDC_IO_LED_UP = 1 << 25,
    SWDC_IO_LED_DOWN = 1 << 24,
    SWDC_IO_LED_LEFT = 1 << 23,
    SWDC_IO_LED_RIGHT = 1 << 22,
};

struct swdc_io_analog_state {
    /* Current steering wheel position, where zero is the centered position.

       The game will accept any signed 16-bit position value, however a real
       cabinet will report a value of approximately +/- 25230 when the wheel
       is at full lock. Steering wheel positions of a magnitude greater than
       this value are not possible on a real cabinet. */

    int16_t wheel;

    /* Current position of the accelerator pedal, where 0 is released. */

    uint16_t accel;

    /* Current position of the brake pedal, where 0 is released. */

    uint16_t brake;
};

/* Get the version of the SWDC IO API that this DLL supports. This
   function should return a positive 16-bit integer, where the high byte is
   the major version and the low byte is the minor version (as defined by the
   Semantic Versioning standard).

   The latest API version as of this writing is 0x0100. */

uint16_t swdc_io_get_api_version(void);

/* Initialize the IO DLL. This is the second function that will be called on
   your DLL, after SWDC_io_get_api_version.

   All subsequent calls to this API may originate from arbitrary threads.

   Minimum API version: 0x0100 */

HRESULT swdc_io_init(void);

/* Get the state of the cabinet's operator buttons as of the last poll. See
   SWDC_IO_OPBTN enum above: this contains bit mask definitions for button
   states returned in *opbtn. All buttons are active-high.

   Minimum API version: 0x0100 */

void swdc_io_get_opbtns(uint8_t *opbtn);

/* Get the state of the cabinet's gameplay buttons as of the last poll. See
   SWDC_IO_GAMEBTN enum above for bit mask definitions. Inputs are split into
   a left hand side set of inputs and a right hand side set of inputs: the bit
   mappings are the same in both cases.

   All buttons are active-high, even though some buttons' electrical signals
   on a real cabinet are active-low.

   Minimum API version: 0x0100 */

void swdc_io_get_gamebtns(uint16_t *gamebtn);

/* Poll the current state of the cabinet's JVS analog inputs. See structure
   definition above for details.

   Minimum API version: 0x0100 */

void swdc_io_get_analogs(struct swdc_io_analog_state *out);

/* Initialize LED emulation. This function will be called before any
   other swdc_io_led_*() function calls.

   All subsequent calls may originate from arbitrary threads and some may
   overlap with each other. Ensuring synchronization inside your IO DLL is
   your responsibility. 
   
   Minimum API version: 0x0101 */

HRESULT swdc_io_led_init(void);

/* Update the FET outputs. rgb is a pointer to an array up to 3 bytes.

   The following bits are used to control the FET outputs:
   [0]: LEFT SEAT LED
   [1]: RIGHT SEAT LED

   The LED is truned on when the byte is 255 and turned off when the byte is 0.

   Minimum API version: 0x0101 */

void swdc_io_led_set_fet_output(uint8_t board, const uint8_t *rgb);

/* Update the RGB LEDs. rgb is a pointer to an array up to 32 * 4 = 128 bytes. 

   The LEDs are laid out as follows:
   [0]: LEFT UP LED
   [1-2]: LEFT CENTER LED
   [3]: LEFT DOWN LED
   [5]: RIGHT UP LED
   [6-7]: RIGHT CENTER LED
   [8]: RIGHT DOWN LED

   Each rgb value is comprised for 4 bytes in the order of R, G, B, Speed.
   Speed is a value from 0 to 255, where 0 is the fastest speed and 255 is the slowest.

   Minimum API version: 0x0101 */

void swdc_io_led_gs_update(uint8_t board, const uint8_t *rgb);

/* Update the cabinet button LEDs. rgb is a pointer to an array up to 6 bytes.

   The LEDs are laid out as follows:
   [0]: START LED
   [1]: VIEW CHANGE LED
   [2]: UP LED
   [3]: DOWN LED
   [4]: RIGHT LED
   [5]: LEFT LED

   The LED is turned on when the byte is 255 and turned off when the byte is 0.

   Minimum API version: 0x0101 */

void swdc_io_led_set_leds(uint8_t board, const uint8_t *rgb);

/* Initialize FFB emulation. This function will be called before any
   other swdc_io_ffb_*() function calls.

   This will always be called even if FFB board emulation is disabled to allow
   the IO DLL to initialize any necessary resources.

   Minimum API version: 0x0102 */

HRESULT swdc_io_ffb_init(void);

/* Toggle FFB emulation. If active is true, FFB emulation should be enabled.
   If active is false, FFB emulation should be disabled and all FFB effects
   should be stopped and released.

   Minimum API version: 0x0102 */

void swdc_io_ffb_toggle(bool active);

/* Set a constant force FFB effect.
   
   Direction is 0 for right and 1 for left.
   Force is the magnitude of the force, where 0 is no force and 127 is the
   maximum force in a given direction.

   Minimum API version: 0x0102 */

void swdc_io_ffb_constant_force(uint8_t direction, uint8_t force);

/* Set a (sine) periodic force FFB effect.
   
   Period is the period of the effect in milliseconds (not sure).
   Force is the magnitude of the force, where 0 is no force and 127 is the
   maximum force.

   Minimum API version: 0x0102 */

void swdc_io_ffb_rumble(uint8_t period, uint8_t force);

/* Set a damper FFB effect.
   
   Force is the magnitude of the force, where 0 is no force and 40 is the
   maximum force. Theoretically the maximum force is 127, but the game only
   uses a maximum of 40.

   Minimum API version: 0x0102 */

void swdc_io_ffb_damper(uint8_t force);
