#include "temp.h"

/**
 This driver implements eight fancy sawtooth-ish pseudo reading.
 It oscillates between -5 and 45 °C.
*/

static uint8_t updown;
static uint8_t delay;
static uint16_t val[8];

void temp_init_dummy(void)
{
    updown = 0;
}

void temp_setup_dummy(uint8_t dev)
{
    val[dev&7] = (20+dev)<<5; // start with different values
}

int16_t temp_poll_dummy(uint8_t dev)
{
    uint8_t p = (dev&7);
    uint8_t mp = 1<<p;
    int16_t v = val[p];

#if 0
    if (dev > 7) {
        if (delay & mp)
            delay &=~ mp;
        else {
            delay |= mp;
            return TEMP_AGAIN;
        }
    }
#endif
    if (updown & mp) {
        v += 1<<3; // 0.25K
        if (v > 45<<5)
            updown &=~ mp;
    } else {
        //v -= ((dev>>3)+1)<<2; // downspeed depends on devnum's high bits
        v -= 1<<2; // downspeed depends on devnum's high bits
        if (v < (-5)<<5)
            updown |= mp;
    }
    val[p] = v;
    return v;
}

