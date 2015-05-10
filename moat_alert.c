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

/* This code implements reading alert pins via 1wire.
 */

#include <avr/pgmspace.h>

#include "moat_internal.h"
#include "dev_data.h"
#include "debug.h"
#include "onewire.h"

#ifdef CONDITIONAL_SEARCH

uint8_t read_alert_len(uint8_t chan)
{
	uint8_t len;
	if(!chan)
		len = TC_MAX;
	if (chan >= TC_MAX)
		next_idle('x');
	else
		len = pgm_read_byte_near(&moat_sizes[chan]);
	return (len+7)>>3;
}

void read_alert(uint8_t chan, uint8_t *buf)
{
	if (chan) { // fill alert bitmap
		alert_fill_fn *ac;
		const moat_call_t *mc;
		if (chan >= TC_MAX)
			next_idle('p');
		mc = &moat_calls[chan];
		ac = pgm_read_ptr_near(&mc->alert_fill);

		ac(buf);
	} else { // all inputs: send bits
		uint8_t b=0,i,mask=1;
		const moat_call_t *mc = moat_calls;

		for(i=0;i<TC_MAX;i++) {
			alert_check_fn *ac = pgm_read_ptr_near(&mc->alert_check);
			if(ac())
				b |= mask;
			mask <<= 1;
			if (!mask) { // byte is full
				*buf++ = b;
				b=0; mask=1;
			}
			mc++;
		}
		if (mask != 1) // residue
			*buf = b;
	}
}

#endif
