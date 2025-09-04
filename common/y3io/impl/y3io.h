#pragma once

#include <stdint.h>
#include <windows.h>

#include "hooklib/y3.h"

/* Get the version of the Y3 IO API that this DLL supports. This
   function should return a positive 16-bit integer, where the high byte is
   the major version and the low byte is the minor version (as defined by the
   Semantic Versioning standard).

   The latest API version as of this writing is 0x0100. */
uint16_t y3_io_get_api_version();

/* Initialize the Y3 board. This function will be called before any other
   y3_io_*() function calls. Errors returned from this function will
   manifest as a disconnected Y3 board.

   This method may be called multiple times.

   Minimum API version: 0x0100 */
HRESULT y3_io_init();

/* Fills the given buffer with cards that are detected on the play field.
   For the values inside CardInfo, see y3.h.
   The input value of numCards is the size of the pCardInfo array.
   The output value of numCards is how many cards (>=0) have been set in pCardInfo.

   Minimum API version: 0x0100 */
HRESULT y3_io_get_cards(struct CardInfo* pCardInfo, int* numCards);