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

/* This code implements the main code of MoaT slaves.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>

#include "onewire.h"
#include "features.h"
#include "moat.h"
#include "dev_data.h"
#include "debug.h"
#include "moat_internal.h"
#include "port.h"
#include "console.h"

#define _1W_READ_GENERIC  0xF2
#define _1W_WRITE_GENERIC 0xF4

void end_transmission(uint16_t crc)
{
	crc = ~crc;
	xmit_byte(crc);
	xmit_byte(crc >> 8);
	{
		uint16_t icrc;
		recv_byte();
		icrc = recv_byte_in();
		recv_byte();
		icrc |= recv_byte_in() << 8;
		if (icrc != ~crc) {
			DBG_P(" crc=");
			DBG_W(crc);
			DBG_P(" icrc=");
			DBG_W(icrc);
			DBG_C(' ');
			next_idle('c');
		}
		// DBG_P("CRC OK ");
	}
}

static inline void
read_config(uint16_t crc)
{
	uint8_t chan;
	cfg_addr_t off;
	uint8_t len;
	chan = recv_byte_in();
	if (chan) {
		cfg_addr(&off, &len, chan);
		//DBG_C('c'); DBG_X(chan);
		if (off == 0) len=0;
		xmit_byte(len);
		crc = crc16(crc,chan);
		crc = crc16(crc,len);
		while(len) {
			uint8_t b = cfg_byte(off++);
			len--;
			xmit_byte(b);
			crc = crc16(crc,b);
		}
	} else { // list of known types
		len = cfg_count(&off);
		xmit_byte(len);
		crc = crc16(crc,chan);
		crc = crc16(crc,len);
		while(len) {
			uint8_t b = cfg_type(&off);
			len--;
			xmit_byte(b);
			crc = crc16(crc,b);
		}
	}
	end_transmission(crc);
}

static void moat_read(void)
{
	uint16_t crc = 0;
	uint8_t dtype;

	/*
	 Implement reading data. We read whatever necessary, write the length,
	 write the data, write the CRC, read the inverted CRC back, and then do
	 whatever necessary to effect the read (e.g. clear a flag, update a
	 stored value, whatever).

	 Typically there's some free time between reading, writing the length,
	 and writing the data respectively. CRC read is immediate, so we
	 pre-calculate as much as possible and add the rest on the go.

	 Do not forget that bus errors and whatnot can abort this code in any
	 recv/xmit call.
	 */
	
	recv_byte();
	crc = crc16(crc,_1W_READ_GENERIC);
	dtype = recv_byte_in();
	//DBG_C('T'); DBG_X(dtype);
	recv_byte();
	crc = crc16(crc,dtype);

	switch(dtype) {
	case TC_CONFIG: read_config(crc); break;
	case TC_CONSOLE: read_console(crc); break;
	case TC_PORT: read_port(crc); break;
	default: DBG_C('?'); return;
	}
}

void moat_write(void) {
}

void do_command(uint8_t cmd)
{
	if(cmd == _1W_READ_GENERIC) {
		//DBG_P(":I");
		moat_read();
	} else if(cmd == _1W_WRITE_GENERIC) {
		//DBG_P(":I");
		moat_write();
	} else {
		DBG(0x0E);
		DBG_P("?CI ");
		DBG_X(cmd);
		set_idle();
	}
}

void update_idle(uint8_t bits)
{
}

void init_state(void)
{
}

#ifdef CONDITIONAL_SEARCH
uint8_t condition_met(void) {
	if(console_alert()) return 1;
	if(port_alert()) return 1;
	return 0; // change_seen;
}
#endif

#if 0 // def HAVE_UART
static unsigned long long x = 0;
#endif
void mainloop(void) {
	DBG(0x1E);
#if 0 // def HAVE_UART
	if(++x<100000ULL) return;
	x = 0;
	DBG_C('/');
#endif
}
