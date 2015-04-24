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

/* This code implements reading the console buffer via 1wire.
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

#ifdef CONSOLE_WRITE
void write_console(uint16_t crc)
{
	uint8_t chan;
	uint8_t len, pos=0;
	uint8_t buf[MAXBUF+1];
	chan = recv_byte_in();
	if (chan != 1)
		next_idle('w');
	recv_byte();
	len = recv_byte_in();
	recv_byte();
	if ((len==0) || (len>MAXBUF))
		next_idle('u');
	crc = crc16(crc,chan);
	crc = crc16(crc,len);
	while(1) {
		uint8_t b = recv_byte_in();
		buf[pos++] = b;
		if (pos == len) {
			crc = crc16(crc,b);
			break;
		}
		recv_byte();
		crc = crc16(crc,b);
	}
	end_transmission(crc);
	buf[pos] = 0;
	console_puts((char *)buf);
}
#endif // console_write

#endif
