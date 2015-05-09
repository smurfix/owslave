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

void read_count(uint16_t crc)
{
	uint8_t chan;
	count_t *t;

	chan = recv_byte_in();
	if (chan) { // one COUNT: send value
		uint8_t buf[sizeof(t->count)], *bp=buf;
		if (chan > N_COUNT)
			next_idle('p');
		t = &counts[chan-1];

		xmit_byte(sizeof(t->count));
		crc = crc16(crc,chan);
		crc = crc16(crc,sizeof(t->count));

		cli();
		switch(sizeof(t->count)) {
		case 4: *bp++ = t->count>>24;
		case 3: *bp++ = t->count>>16;
		case 2: *bp++ = t->count>>8;
		case 1: *bp++ = t->count;
		}
		t->flags &=~ CF_IS_ALERT;
		sei();

		crc = xmit_bytes_crc(crc,buf,sizeof(t->count));
	} else { // all COUNTs: send port state, time remaining
#define BLEN N_COUNT*sizeof(t->count)
		uint8_t buf[BLEN],*bp=buf;
		uint8_t i;
		t = counts;

		xmit_byte(BLEN);
		crc = crc16(crc,0);
		crc = crc16(crc,BLEN);

		for(i=0;i<N_COUNT;i++,t++) {
			cli();
			switch(sizeof(t->count)) {
			case 4: *bp++ = t->count>>24;
			case 3: *bp++ = t->count>>16;
			case 2: *bp++ = t->count>>8;
			case 1: *bp++ = t->count;
			}
			sei();
		}
		crc = xmit_bytes_crc(crc, buf,BLEN);
	}
	end_transmission(crc);
}

#endif
