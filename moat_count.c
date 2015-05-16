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

#include <string.h> // memset

#include "moat_internal.h"
#include "dev_data.h"
#include "debug.h"
#include "onewire.h"
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
#if COUNT_SIZE >= 4
		*buf++ = t->count>>24;
#endif
#if COUNT_SIZE >= 3
		*buf++ = t->count>>16;
#endif
#if COUNT_SIZE >= 2
		*buf++ = t->count>>8;
#endif
		*buf++ = t->count;
		t->flags &=~ CF_IS_ALERT;
		sei();
	} else { // all COUNTs
		uint8_t i;
		t = counts;

		for(i=0;i<N_COUNT;i++,t++) {
			cli();
#if COUNT_SIZE >= 4
			*buf++ = t->count>>24;
#endif
#if COUNT_SIZE >= 3
			*buf++ = t->count>>16;
#endif
#if COUNT_SIZE >= 2
			*buf++ = t->count>>8;
#endif
			*buf++ = t->count;
			sei();
		}
	}
}

#ifdef CONDITIONAL_SEARCH

char alert_count_check(void)
{
	return (count_changed_cache+7)>>3;
}

void alert_count_fill(uint8_t *buf)
{
	uint8_t i;
	count_t *t = counts;
	uint8_t m=1;

	memset(buf,0,(N_COUNT+7)>>3);
	for(i=0;i < N_COUNT; i++,t++) {
		if (!m) {
			m = 1;
			buf++;
		}
		if (t->flags & CF_IS_ALERT)
			*buf |= m;
		m <<= 1;
	}
}

#endif // conditional



#endif
