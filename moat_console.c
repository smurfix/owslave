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

/* This code implements the DS2423 counter (obsolete and no-longer-produced).
 * Only the features necessary for reading the physical counters are
 * implemented; the rest is of no particular interest. Yet.
 */

/* Input pins are PA0 thru PA7, depending on the definition of NCOUNTERS.
   If ANALOG is defined, these pins are read by the ADC and an adaptive
   hysteresis is used to trigger the counters, otherwise they are used
   as straight digital inputs. */

/* if SLOW is defined, a decaying average (factor 2^-SLOW) is used to
   lowpass-filter the (analog) result. Use this if you want to monitor e.g.
   a blinking LED, while not being distracted by ambient neon lighting.
   Note that sample frequency is approx. 8 kHz / NCOUNTERS.
 */

#include "moat_internal.h"
#include "dev_data.h"
#include "debug.h"
#include "onewire.h"
#include "console.h"

#ifdef N_CONSOLE

#define MAXBUF 32
void read_console(uint16_t crc)
{
	uint8_t chan;
	uint8_t len, sent=0;
	chan = recv_byte_in();
	if (chan) {
		uint8_t buf[MAXBUF];

		if (chan != 1)
			next_idle('W');
		len = console_buf_len();
		if (len>MAXBUF)
			len=MAXBUF;
		xmit_byte(len);
		crc = crc16(crc,chan);
		crc = crc16(crc,len);
		len = console_buf_read(buf,len);
		while(len) {
			uint8_t b = buf[sent++];
			len--;
			xmit_byte(b);
			crc = crc16(crc,b);
		}
	} else { // list of channels and their length
		len = console_buf_len();
		if (len) {
			xmit_byte(2);
			crc = crc16(crc,chan);
			crc = crc16(crc,2);
			xmit_byte(1);
			crc = crc16(crc,1);
			xmit_byte(len);
			crc = crc16(crc,len);
		} else {
			xmit_byte(0);
			crc = crc16(crc,chan);
			crc = crc16(crc,0);
		}
	}
	end_transmission(crc);
	if (sent)
		console_buf_done(sent);
}

#endif
