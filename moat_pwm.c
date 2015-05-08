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
#include "pwm.h"
#include "timer.h"

#ifdef N_PWM

void read_pwm(uint16_t crc)
{
	uint8_t chan;
	pwm_t *t;
	port_t *p;

	chan = recv_byte_in();
	if (chan) { // one PWM: send port state, remaining time, timer values
		uint8_t buf[7], *bp=buf;
		uint16_t tm;
		if (chan > N_PWM)
			next_idle('p');
		t = &pwms[chan-1];
		p = &ports[chan-1];

		xmit_byte(7);
		crc = crc16(crc,chan);
		crc = crc16(crc,7);

		*bp++ = p->flags;
		tm=timer_remaining(&t->timer);
		*bp++ = tm>>8;
		*bp++ = tm;
		*bp++ = t->t_on>>8;
		*bp++ = t->t_on;
		*bp++ = t->t_off>>8;
		*bp++ = t->t_off;
		crc = xmit_bytes_crc(crc,buf,7);
	} else { // all PWMs: send port state, time remaining
#define BLEN N_PWM*2+(N_PWM+7)/8
		uint8_t buf[BLEN],*bp=buf;
		uint8_t i;
		t = pwms;
		p = ports;

		xmit_byte(BLEN);
		crc = crc16(crc,0);
		crc = crc16(crc,BLEN);

		for(i=0;i<N_PWM;i++) {
			uint16_t tm;
			if(!(i&7)) {
				uint8_t j,v=0,m=1;
				for(j=0;j<8;j++) {
					if (i+j >= N_PWM) break;
					if (p->flags & PFLG_CURRENT)
						v |= m;
					m <<= 1;
				}
				*bp++ = v;
			}
			tm=timer_remaining(&t->timer);
			*bp++ = tm>>8;
			*bp++ = tm;

			t++; p++;
		}
		crc = xmit_bytes_crc(crc, buf,BLEN);
	}
	end_transmission(crc);
}

void write_pwm(uint16_t crc)
{
	uint8_t chan;
	uint8_t len;
	pwm_t *t;
	uint8_t buf[4];
	uint16_t a,b;
	char startup;

	chan = recv_byte_in();
	recv_byte();
	len = recv_byte_in();
	recv_byte();

	crc = crc16(crc, chan);
	crc = crc16(crc, len);

	if (chan == 0 || chan > N_PWM || (len != 2 && len != 4))
		next_idle('w');
	t = &pwms[chan-1];

	crc = recv_bytes_crc(crc,buf,len);
	end_transmission(crc);
	startup = !(t->is_on ? t->t_on : t->t_off);
	if (len == 2) {
		a = buf[0];
		a |= a<<8;
		b = buf[1];
		b |= b<<8;
	} else {
		a = buf[0]<<8|buf[1];
		b = buf[2]<<8|buf[3];
	}
	t->t_on = a;
	t->t_off = b;
	if(startup)
		timer_start(1, &t->timer);
}

#endif
