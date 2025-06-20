#include <stdbool.h>
#include <stdint.h>

#include "idacio/shifter.h"

static bool idac_shifter_shifting;
static uint8_t idac_shifter_gear;

void idac_shifter_set(uint8_t gear)
{
    idac_shifter_gear = gear;
}

void idac_shifter_update(bool shift_dn, bool shift_up)
{
    if (!idac_shifter_shifting) {
        if (shift_dn && idac_shifter_gear > 0) {
            idac_shifter_gear--;
        }

        if (shift_up && idac_shifter_gear < 6) {
            idac_shifter_gear++;
        }
    }

    idac_shifter_shifting = shift_dn || shift_up;
}

uint8_t idac_shifter_current_gear(void)
{
    return idac_shifter_gear;
}
