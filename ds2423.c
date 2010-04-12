/*
 *  Copyright © 2010, Matthias Urlichs <matthias@urlichs.de>
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
 * implemented; the rest is of no particular interest.
 */

#include "onewire.h"

#define C_WRITE_SCRATCHPAD 0x0F // TODO
#define C_READ_SCRATCHPAD  0xAA // TODO
#define C_COPY_SCRATCHPAD  0x55 // TODO
#define C_READ_MEM         0xF0 // TODO
#define C_READ_MEM_COUNTER 0xA5 // TODO

static uint32_t counterA = 0;
static uint32_t counterB = 0;

void do_mem_counter(void)
{
	uint16_t crc = 0;
	uint8_t b;
	uint8_t len;
	uint16_t adr;

	/*
	 The following code does:
         * receive address (2 bytes), add them to CRC
         * send 1..32 bytes (0xFF), add them to CRC
	 * send counter (4 bytes, CRC)
	 * send 4 zero bytes
	 * send inverted CRC
         This is all very straightforward, except that the CRC calculation
	 is delayed somewhat: the time between recv_byte_in() and xmit_byte()
	 is only a bit wide, which may not be enough time to add a CRC byte.
	 */
	
	recv_byte();
	crc = crc16(crc,C_READ_MEM_COUNTER);
	b = recv_byte_in();
	recv_byte();
	crc = crc16(crc,b);
	adr = b;
	b = recv_byte_in();
	adr |= b<<8;
	if(1) {
		xmit_byte(0xFF);
		crc = crc16(crc,b);
	} else {
xmore:
		xmit_byte(0xFF);
	}
	len = 0x1F - (adr & 0x1F);
	adr++;
	crc = crc16(crc,0xFF);

	while(len) {
		xmit_byte(0xFF);
		crc = crc16(crc,0xFF);
		adr++;
		len--;
	}
#define SEND(_x) do {                                        \
		uint32_t x = (_x);                           \
		b = x    ; xmit_byte(b); crc = crc16(crc,b); \
		b = x>> 8; xmit_byte(b); crc = crc16(crc,b); \
		b = x>>16; xmit_byte(b); crc = crc16(crc,b); \
		b = x>>24; xmit_byte(b); crc = crc16(crc,b); \
		} while(0)
	if ((adr&0x1FF) == 0x1E0) {
		counterA++;
		SEND(counterA);
	}
	else if ((adr&0x1FF) == 0x000) {
		counterB++;
		SEND(counterB);
	}
	else {
		DBG_C('@');
		DBG_Y(adr);
		SEND(0xFFFFFFFF);
	}
	SEND(0);
#undef SEND

	crc = ~crc;
	xmit_byte(crc);
	xmit_byte(crc >> 8);
	if(adr & 0x1FF) {
		crc = 0;
		goto xmore;
	}
	while(1)
		xmit_bit(1);
}

void do_command(uint8_t cmd)
{
	if(cmd == C_READ_MEM_COUNTER) {
		DBG_P(":I");
		do_mem_counter();
	} else {
		DBG_P("?CI");
		DBG_X(cmd);
		set_idle();
	}
}

void update_idle(uint8_t bits)
{
	//DBG_C('\\');
}

void init_state(void)
{
}

