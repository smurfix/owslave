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

#include <string.h> // memset

#include "moat_internal.h"
#include "dev_data.h"
#include "debug.h"
#include "onewire.h"
#include "port.h"
#include "pwm.h"
#include "timer.h"

#ifdef N_PWM
#define BLEN N_PWM*2+(N_PWM+7)/8

uint8_t read_pwm_len(uint8_t chan)
{
	if(chan)
		return 7;
	else
		return BLEN;
}
void read_pwm(uint8_t chan, uint8_t *buf)
{
	pwm_t *t;
	port_t *p;

	if (chan) { // one PWM: send port state, remaining time, timer values
		uint16_t tm;
		if (chan > N_PWM)
			next_idle('p');
		t = &pwms[chan-1];
		p = &ports[t->port-1];
		tm=timer_remaining(&t->timer);

		*buf++ = p->flags;
		*buf++ = tm>>8;
		*buf++ = tm;
		*buf++ = t->t_on>>8;
		*buf++ = t->t_on;
		*buf++ = t->t_off>>8;
		*buf++ = t->t_off;
	} else { // all PWMs: send port state, time remaining
		uint8_t i;
		t = pwms;

		for(i=0;i<N_PWM;i++,t++) {
			uint16_t tm;
			if(!(i&7)) {
				p = &ports[t->port-1];
				uint8_t j,v=0,m=1;
				for(j=0;j<8;j++) {
					if (i+j >= N_PWM) break;
					if (p->flags & PFLG_CURRENT)
						v |= m;
					m <<= 1;
				}
				*buf++ = v;
			}
			tm=timer_remaining(&t->timer);
			*buf++ = tm>>8;
			*buf++ = tm;
		}
	}
}

void write_pwm(uint16_t crc)
{
	uint8_t chan;
	uint8_t len;
	pwm_t *t;
	uint8_t buf[4];
	uint16_t a,b,last;

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
	last = ((t->flags & PWM_IS_ON) ? t->t_on : t->t_off);
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
	if(!last || (t->flags & PWM_FORCE))
		timer_reset(&t->timer);
}

#ifdef CONDITIONAL_SEARCH

extern uint8_t pwm_changed_cache;

char alert_pwm_check(void)
{
	return pwm_changed_cache;
}

void alert_pwm_fill(uint8_t *buf)
{
	uint8_t i;
	pwm_t *t = pwms;

	memset(buf,0,(N_PWM+7)>>3);
	for(i=0;i < N_PWM; i++,t++)
		if (t->flags & PWM_IS_ALERT)
			buf[i>>3] |= 1<<(i&7);
}

#endif // conditional



#endif
