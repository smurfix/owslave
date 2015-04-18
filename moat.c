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

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>

#include "onewire.h"
#include "features.h"
#include "moat.h"
#include "dev_data.h"
#include "debug.h"

#define _1W_READ_GENERIC  0xF2
#define _1W_WRITE_GENERIC 0xF4


void do_read(void)
{
	uint16_t crc = 0;
	uint8_t dtype,chan;

	/*
	 The following code does:
         * receive address (2 bytes), add them to CRC
         * send 1..32 bytes (0xFF), add them to CRC
	 * send counter (4 bytes, CRC)
	 * send 4 zero bytes
	 * send inverted CRC
	   This is all very straightforward, except that the CRC calculation
	   for the received address is delayed somewhat: the available time
	   between the second recv_byte_in() and xmit_byte() is less than a bit
	   wide. That may not be enough time to update the CRC.
	 */
	
	recv_byte();
	crc = crc16(crc,_1W_READ_GENERIC);
	dtype = recv_byte_in();
	recv_byte();
	if (dtype == TC_NAME) {
		uint8_t off,len;
		cfg_addr(&off, &len, CfgID_name);
		if (!off) return;
		chan = recv_byte_in();
		xmit_byte(len);
		crc = crc16(crc,dtype);
		crc = crc16(crc,chan);
		crc = crc16(crc,len);
		while(len) {
			uint8_t b = cfg_byte(off++);
			len--;
			xmit_byte(b);
			crc = crc16(crc,b);
		}

	} else {
		recv_byte();
		crc = crc16(crc,dtype);
		chan = recv_byte_in();
		//recv_byte();
		crc = crc16(crc,chan);
		//b = recv_byte_in();
	}
	crc = ~crc;
	xmit_byte(crc);
	xmit_byte(crc >> 8);
	{
		uint16_t icrc;
		recv_byte();
		icrc = recv_byte_in();
		recv_byte();
		icrc |= (recv_byte_in() << 8);
		if (icrc != ~crc)
			return; // ERROR
	}
	// data received correctly
}

void do_write(void) {
}

void do_command(uint8_t cmd)
{
	if(cmd == _1W_READ_GENERIC) {
		//DBG_P(":I");
		do_read();
	} else if(cmd == _1W_WRITE_GENERIC) {
		//DBG_P(":I");
		do_write();
	} else {
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
	return 0; // change_seen;
}
#endif

void mainloop(void) {
	while(1) onewire_poll();
}
