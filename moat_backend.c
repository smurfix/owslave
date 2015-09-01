/*
 *  Copyright © 2014-2015, Matthias Urlichs <matthias@urlichs.de>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License (included; see the file LICENSE)
 *  for more details.
 */

/* This file implements the data structures required to hook individual
 * MoaT features (which can be dynamically updated) to the main code
 * (which cannot be).
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "pgm.h"
#include <string.h>

#include "onewire.h"
#include "features.h"
#include "moat.h"
#include "dev_data.h"
#include "debug.h"
#include "moat_internal.h"
#include "port.h"
#include "pwm.h"
#include "count.h"
#include "console.h"
#include "timer.h"
#include "moat_dummy.c"

#define _1W_READ_GENERIC  0xF2
#define _1W_WRITE_GENERIC 0xF4

#define TC_DEFINE(_s) ALIASDEFS(_s)
#include "_def.h"
#undef TC_DEFINE

#define TC_DEFINE(_s) FUNCPTRS(_s),
const moat_call_t moat_calls[TC_MAX] __attribute__((progmem)) = {
#include "_def.h"
};
#undef ALERT_DEF
#undef TC_DEFINE

const uint8_t moat_sizes[] __attribute__ ((progmem)) = {
#include "_nums.h"
};

#ifdef USE_BOOTLOADER
extern uint8_t __mdata_start;
extern uint8_t __mdata_end;
extern uint8_t __mdata_load_start;
extern uint8_t __mbss_start;
extern uint8_t __mbss_end;

void moat_backend_init(void)
{
    uint8_t *dsrc = &__mdata_load_start;
    uint8_t *ddst = &__mdata_load_start;
    while (ddst != &__mdata_end)
        *ddst++ = *dsrc++;
    ddst = &__mbss_start;
    while (ddst != &__mbss_end)
        *ddst++ = 0;
}

const moat_loader_t moat_loader __attribute__ ((progmem,section(".progmem.first"))) = {
    {'M','T'},
    &moat_backend_init,
    moat_calls,
    TC_MAX,
};
#endif

