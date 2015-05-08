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

/* This code implements pulse width modulation.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>

#include "port.h"
#include "pwm.h"
#include "features.h"
#include "moat.h"
#include "dev_data.h"
#include "debug.h"
#include "timer.h"
#include "moat_internal.h"

#ifdef N_PWM

pwm_t pwms[N_PWM];

void pwm_poll(void)
{
	uint8_t i;
	pwm_t *t = pwms;
	port_t *p = ports;

	for(i=0;i<N_PWM;i++) {
		uint8_t tx = t->is_on ? t->t_on : t->t_off;
		if(tx == 0)
			continue;
		if(timer_done(&t->timer)) {
			t->is_on = !t->is_on;
			port_set(p,t->is_on);
			tx = t->is_on ? t->t_on : t->t_off;
			if (tx)
				timer_start(tx-timer_remaining(&t->timer),&t->timer);
		}
		t++; p++;
	}
}

void pwm_init(void)
{
	pwm_t *t = pwms;
	port_t *p = ports;
	uint8_t i;

	for(i=0;i<N_PWM;i++) {
		t->is_on = 0;
		// t->t_on = t->t_off = 0;
		port_set(p,0);
		if(t->t_off)
			timer_start(t->t_off, &t->timer);
		t++; p++;
	}
}


#endif // any PWM controllers
