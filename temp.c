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

/* This code implements basic temp input+output features.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "pgm.h"
#include <string.h>

#include "temp.h"
#include "features.h"
#include "moat.h"
#include "dev_data.h"
#include "debug.h"
#include "moat_internal.h"

#ifdef N_TEMP

temp_t temps[] = {
#include "_temp.h"
};

#define TEMP_TC_DEFINE(_s) TFUNCPTRS(_s),
static const temp_call_t temp_calls[N_TEMP_DRIVER] __attribute__((progmem)) = {
#include "_temp_defs.h"
};
#undef TEMP_TC_DEFINE

/* Each mainloop pass checks one temp. */
static uint8_t poll_this = 0;
static uint8_t poll_step = 0;
static uint8_t max_seen = 0;
/* Number of highest temp that has a change +1  */
uint8_t temp_changed_cache;

void poll_temp(void)
{
	uint8_t i = poll_this;
	temp_t *tt;
	int16_t temp;
	const temp_call_t *tc;
	temp_poll_fn *tfp;

	if (i >= N_TEMP) {
		i=0;
		temp_changed_cache = max_seen;
		max_seen=0;
	}
	tt = &temps[i];
	tc = &temp_calls[tt->flags & TEMP_MASK];
	tfp = pgm_read_ptr(&tc->poll);
	temp = tfp(tt->device);
	if (temp == TEMP_AGAIN)
		return;

	i += 1;
	if (tt->flags & TEMP_ALERT) {
		if (tt->lower != 0x7FFF && tt->value <= tt->lower)
			tt->flags |= TEMP_IS_ALERT_L;
		if (tt->upper != 0x8000 && tt->value >= tt->upper)
			tt->flags |= TEMP_IS_ALERT_H;
	}
	if (tt->flags & (TEMP_IS_ALERT_L|TEMP_IS_ALERT_H))
		max_seen = i;
	poll_this=i;
}

void init_temp(void)
{
	const temp_call_t *tc = temp_calls;
	temp_t *tt = temps;
	uint8_t i;

	for(i=0;i<N_TEMP_DRIVER;i++,tc++) {
		temp_init_fn *tfi = pgm_read_ptr(&tc->init);
		tfi();
	}
	for(i=0;i<N_TEMP;i++,tt++) {
		tc = &temp_calls[tt->flags & TEMP_MASK];
		temp_setup_fn *tfs = pgm_read_ptr(&tc->setup);
		tfs(tt->device);
	}
}


#endif // any temps
