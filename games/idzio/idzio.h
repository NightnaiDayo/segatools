#pragma once

/* INITIAL D ZERO CUSTOM IO API

   This API definition allows custom driver DLLs to be defined for the
   emulation of Initial D Zero cabinets. To be honest, there is very
   little reason to want to do this, since driving game controllers are a
   mostly-standardized PC peripheral which can be adequately controlled by the
   built-in DirectInput and XInput support in idzhook. However, previous
   versions of Segatools broke this functionality out into a separate DLL just
   like all of the other supported games, so in the interests of maintaining
   backwards compatibility we provide the option to load custom IDZIO
   implementations as well. */

#include <windows.h>

#include <stdbool.h>
#include <stdint.h>

enum {
    IDZ_IO_OPBTN_TEST = 0x01,
    IDZ_IO_OPBTN_SERVICE = 0x02,
};

enum {
    IDZ_IO_GAMEBTN_UP = 0x01,
    IDZ_IO_GAMEBTN_DOWN = 0x02,
    IDZ_IO_GAMEBTN_LEFT = 0x04,
    IDZ_IO_GAMEBTN_RIGHT = 0x08,
    IDZ_IO_GAMEBTN_START = 0x10,
    IDZ_IO_GAMEBTN_VIEW_CHANGE = 0x20,
};

enum {
    /* These are the bitmasks to use when checking which
       lights are triggered on incoming IO4 GPIO writes. */
    IDZ_IO_LED_START        = 1 << 7, 
    IDZ_IO_LED_VIEW_CHANGE  = 1 << 6, 
    IDZ_IO_LED_UP           = 1 << 1, 
    IDZ_IO_LED_DOWN         = 1 << 0, 
    IDZ_IO_LED_RIGHT        = 1 << 14,
    IDZ_IO_LED_LEFT         = 1 << 15 
};

struct idz_io_analog_state {
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

/* Get the version of the IDZ IO API that this DLL supports. This
   function should return a positive 16-bit integer, where the high byte is
   the major version and the low byte is the minor version (as defined by the
   Semantic Versioning standard).

   The latest API version as of this writing is 0x0100. */

uint16_t idz_io_get_api_version(void);

/* Initialize JVS-based input. This function will be called before any other
   idz_io_jvs_*() function calls. Errors returned from this function will
   manifest as a disconnected JVS bus.

   All subsequent calls may originate from arbitrary threads and some may
   overlap with each other. Ensuring synchronization inside your IO DLL is
   your responsibility.

   Minimum API version: 0x0100 */

HRESULT idz_io_jvs_init(void);

/* Poll the current state of the cabinet's JVS analog inputs. See structure
   definition above for details.

   Minimum API version: 0x0100 */

void idz_io_jvs_read_analogs(struct idz_io_analog_state *out);

/* Poll the current state of the cabinet's JVS input buttons and return them
   through the opbtn and gamebtn out parameters. See enum definitions at the
   top of this file for a list of bit masks to be used with these out
   parameters.

   Minimum API version: 0x0100 */

void idz_io_jvs_read_buttons(uint8_t *opbtn, uint8_t *gamebtn);

/* Poll the current position of the six-speed shifter and return it via the
   gear out parameter. Valid values are 0 for neutral and 1-6 for gears 1-6.

   idzhook internally translates this gear position value into the correct
   combination of Gear Left, Gear Right, Gear Up, Gear Down buttons that the
   game will then interpret as the current position of the gear lever.

   Minimum API version: 0x0100 */

void idz_io_jvs_read_shifter(uint8_t *gear);

/* Read the current state of the coin counter. This value should be incremented
   for every coin detected by the coin acceptor mechanism. This count does not
   need to persist beyond the lifetime of the process.

   Minimum API version: 0x0100 */

void idz_io_jvs_read_coin_counter(uint16_t *total);

/* Initialize LED emulation. This function will be called before any
   other idz_io_led_*() function calls.

   All subsequent calls may originate from arbitrary threads and some may
   overlap with each other. Ensuring synchronization inside your IO DLL is
   your responsibility. 
   
   Minimum API version: 0x0101 */

HRESULT idz_io_led_init(void);

/* Update the FET outputs. rgb is a pointer to an array up to 3 bytes.

   The following bits are used to control the FET outputs:
   [0]: LEFT SEAT LED
   [1]: RIGHT SEAT LED

   The LED is truned on when the byte is 255 and turned off when the byte is 0.

   Minimum API version: 0x0101 */

void idz_io_led_set_fet_output(uint8_t board, const uint8_t *rgb);

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

void idz_io_led_gs_update(uint8_t board, const uint8_t *rgb);

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

void idz_io_led_set_leds(uint8_t board, const uint8_t *rgb);

/* Initialize FFB emulation. This function will be called before any
   other idz_io_ffb_*() function calls.

   This will always be called even if FFB board emulation is disabled to allow
   the IO DLL to initialize any necessary resources.

   Minimum API version: 0x0102 */

HRESULT idz_io_ffb_init(void);

/* Toggle FFB emulation. If active is true, FFB emulation should be enabled.
   If active is false, FFB emulation should be disabled and all FFB effects
   should be stopped and released.

   Minimum API version: 0x0102 */

void idz_io_ffb_toggle(bool active);

/* Set a constant force FFB effect.
   
   Direction is 0 for right and 1 for left.
   Force is the magnitude of the force, where 0 is no force and 127 is the
   maximum force in a given direction.

   Minimum API version: 0x0102 */

void idz_io_ffb_constant_force(uint8_t direction, uint8_t force);

/* Set a (sine) periodic force FFB effect.
   
   Period is the period of the effect in milliseconds (not sure).
   Force is the magnitude of the force, where 0 is no force and 127 is the
   maximum force.

   Minimum API version: 0x0102 */

void idz_io_ffb_rumble(uint8_t period, uint8_t force);

/* Set a damper FFB effect.
   
   Force is the magnitude of the force, where 0 is no force and 40 is the
   maximum force. Theoretically the maximum force is 127, but the game only
   uses a maximum of 40.

   Minimum API version: 0x0102 */

void idz_io_ffb_damper(uint8_t force);
