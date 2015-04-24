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

#ifdef N_PORT

void read_port(uint16_t crc)
{
	uint8_t chan;
	chan = recv_byte_in();
	t_port *portp;
	if (chan) { // one input: send flags, mark as scanned
		if (chan > N_PORT)
			next_idle('p');
		portp = &ports[chan-1];
		_P_VARS(portp)

		port_pre_send(portp);

		xmit_byte(1);
		crc = crc16(crc,chan);
		crc = crc16(crc,1);
		xmit_byte(flg);
		crc = crc16(crc,flg);
	} else { // all inputs: send bits
		uint8_t b=0,i,mask=1;
		uint8_t len;
		portp = ports;
		len = (N_PORT+7)>>3;
		xmit_byte(len);
		crc = crc16(crc,chan);
		crc = crc16(crc,len);

		for(i=0;i<N_PORT;i++) {
			if(portp->flags & PFLG_CURRENT)
				b |= mask;
			mask <<= 1;
			if (!mask) { // byte is full
				xmit_byte(b);
				crc = crc16(crc,b);
				b=0; mask=1;
			}
			portp++;
		}
		if (mask != 1) { // residue
			xmit_byte(b);
			crc = crc16(crc,b);
		}
	}
	end_transmission(crc);
	if (chan) {
		port_post_send(portp);
		moat_dump_port(chan);
	}
}

void moat_dump_port(char chan) {
	t_port *pp = &ports[chan-1];

	_P_VARS(pp)

	DBG_P("Ch");DBG_X(chan);
	DBG_C('.') ;DBG_X(pp->adr);
	DBG_P(" f");DBG_X(pp->flags);
	DBG_P(" =");DBG_X(*pin);
	DBG_NL();
}

#endif
