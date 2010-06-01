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

/* This code implements (some of) the DS2408 8-bit I/O (obsolete).
 */

#include "onewire.h"

#define C_READ_PIO         0xF0
#define C_READ_CHANNEL     0xF5 // TODO
#define C_WRITE_CHANNEL    0x5A // TODO
#define C_WRITE_CONDSEARCH 0xCC // TODO
#define C_RESET_LATCHES    0xC3 // TODO

static u_char
	pio_logic          = 0xFF,
	pio_output_latch   = 0xFF,
	pio_activity_latch = 0x00,
	cond_search_mask   = 0x00,
	cond_polarity_mask = 0x00,
	control_status     = 0x88;

void do_read_pio(void)
{
	u_short crc = 0;
	u_short adr;
	u_char bcrc = 1;
	u_char b;

	/*
	 The following code does:
         * receive address (2 bytes), add them to CRC
         This is all very straightforward, except that the CRC calculation
	 is delayed somewhat: the time between recv_byte_in() and xmit_byte()
	 is only a bit wide, which may not be enough time to add a CRC byte.
	 */
	
	recv_byte();
	crc = crc16(crc,0xF0);
	b = recv_byte_in();
	recv_byte();
	crc = crc16(crc,b);
	adr = b;
	b = recv_byte_in();
	adr |= b<<8;
#define XMIT(val) do {                                     \
		xmit_byte(val);                            \
		if(bcrc) { crc = crc16(crc,b); bcrc = 0; } \
		crc = crc16(crc,val);                      \
	} while(0)

	while(adr < 0x88) { XMIT(0); adr++; }
	switch(adr) {
	case 0x88: XMIT(pio_logic);
	case 0x89: XMIT(pio_output_latch);
	case 0x8A: XMIT(pio_activity_latch);
	case 0x8B: XMIT(cond_search_mask);
	case 0x8C: XMIT(cond_polarity_mask);
	case 0x8D: XMIT(control_status);
	case 0x8E: XMIT(0xFF);
	case 0x8F: XMIT(0xFF);
		crc = ~crc;
		xmit_byte(crc);
		xmit_byte(crc >> 8);
	default:
		while(1)
			XMIT(0xFF);
	}
}

void do_command(u_char cmd)
{
	if(cmd == C_READ_PIO) {
		DBG_P(":I");
		do_read_pio();
	} else {
		DBG_P("?CI");
		DBG_X(cmd);
		set_idle();
	}
}

void update_idle(u_char bits)
{
	//DBG_C('\\');
	uart_try_send();
}

void init_state(void)
{
}

