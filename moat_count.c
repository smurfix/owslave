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

/* This code implements reading port pins via 1wire.
 */

#include "moat_internal.h"
#include "dev_data.h"
#include "debug.h"
#include "onewire.h"
#include "port.h"
#include "count.h"
#include "timer.h"

#ifdef N_COUNT
#define BLEN N_COUNT*sizeof(t->count)

uint8_t read_count_len(uint8_t chan)
{
	count_t *t;
	if(chan)
		return sizeof(t->count);
	else
		return BLEN;
}

void read_count(uint8_t chan, uint8_t *buf)
{
	count_t *t;

	if (chan) { // one COUNT: send value
		if (chan > N_COUNT)
			next_idle('p');
		t = &counts[chan-1];

		cli();
		switch(sizeof(t->count)) {
		case 4: *buf++ = t->count>>24;
		case 3: *buf++ = t->count>>16;
		case 2: *buf++ = t->count>>8;
		case 1: *buf++ = t->count;
		}
		t->flags &=~ CF_IS_ALERT;
		sei();
	} else { // all COUNTs
		uint8_t i;
		t = counts;

		for(i=0;i<N_COUNT;i++,t++) {
			cli();
			switch(sizeof(t->count)) {
			case 4: *buf++ = t->count>>24;
			case 3: *buf++ = t->count>>16;
			case 2: *buf++ = t->count>>8;
			case 1: *buf++ = t->count;
			}
			sei();
		}
	}
}

#endif
