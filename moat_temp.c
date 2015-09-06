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

/* This code implements reading temp pins via 1wire.
 */

#include <string.h>

#include "moat_internal.h"
#include "dev_data.h"
#include "debug.h"
#include "onewire.h"
#include "temp.h"

#ifdef N_TEMP

uint8_t read_temp_len(uint8_t chan)
{
	if(chan)
		return 7;
	else
		return N_TEMP*2;
}

void read_temp(uint8_t chan, uint8_t *buf)
{
	temp_t *tempp;
	if (chan) { // one input: send flags, mark as scanned
		if (chan > N_TEMP)
			next_idle('p');
		tempp = &temps[chan-1];
		*buf++ = tempp->flags;
		*buf++ = tempp->value>>8;
		*buf++ = tempp->value&0xFF;
		*buf++ = tempp->lower>>8;
		*buf++ = tempp->lower&0xFF;
		*buf++ = tempp->upper>>8;
		*buf++ = tempp->upper&0xFF;
	} else { // all inputs: send values
		uint8_t i;
		tempp = temps;

		for(i=0;i<N_TEMP;i++) {
			*buf++ = tempp->value>>8;
			*buf++ = tempp->value&0xFF;
			tempp++;
		}
	}
}

void read_temp_done(uint8_t chan) {
	temp_t *tempp;
	if(!chan) return;
	tempp = &temps[chan-1];
	tempp->flags &=~ (TEMP_IS_ALERT_L|TEMP_IS_ALERT_H);
}

void write_temp_check(uint8_t chan, uint8_t *buf, uint8_t len)
{

	if (chan == 0 || chan > N_TEMP)
		next_idle('p');

	if (len != 2 && len != 4)
		next_idle('a');
}

void write_temp(uint8_t chan, uint8_t *buf, uint8_t len)
{
	uint8_t x;
	uint16_t lower,upper;
	temp_t *tempp = &temps[chan-1];
	if (len == 2) {
		x = *buf++;
		lower = (x<<8)|x;
		x = *buf++;
		upper = (x<<8)|x;
	} else {
		lower = (*buf++)<<8;
		lower |= *buf++;
		upper = (*buf++)<<8;
		upper |= *buf++;
	}
	tempp->lower = lower;
	tempp->upper = upper;
	tempp->flags &=~ (TEMP_IS_ALERT_L|TEMP_IS_ALERT_H);
}

#ifdef CONDITIONAL_SEARCH

char alert_temp_check(void)
{
	return (temp_changed_cache*2 +7)>>3;
}

void alert_temp_fill(uint8_t *buf)
{
	uint8_t i;
	temp_t *t = temps;
	uint8_t m=1;

    DBG_X(temp_changed_cache);
	memset(buf,0,(N_TEMP*2 +7)>>3);
	for(i=0;i < N_TEMP; i++,t++) {
		if (!m) {
			m = 1;
			buf++;
		}
		if (t->flags & TEMP_IS_ALERT_L)
			*buf |= m;
		m <<= 1;
		if (t->flags & TEMP_IS_ALERT_H)
			*buf |= m;
		m <<= 1;
	}
}

#endif // conditional


#endif
