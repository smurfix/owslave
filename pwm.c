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
#include "pgm.h"
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

pwm_t pwms[] = {
#include "_pwm.h"
};

#ifdef CONDITIONAL_SEARCH
uint8_t pwm_changed_cache;
#endif

void poll_pwm(void)
{
	uint8_t i;
	pwm_t *t = pwms;
	port_t *p;
#ifdef CONDITIONAL_SEARCH
	uint8_t max_seen = 0;
#endif

	for(i=0;i<N_PWM;t++) {
		i++;
		uint16_t tx = (t->flags & PWM_IS_ON) ? t->t_on : t->t_off;
		if(tx == 0)
			continue;
		if(timer_done(&t->timer)) {
			p = &ports[t->port-1];
			t->flags ^= PWM_IS_ON;
			port_set(p, t->flags & PWM_IS_ON);
			tx = (t->flags & PWM_IS_ON) ? t->t_on : t->t_off;
			if (tx)
				timer_start(tx-timer_remaining(&t->timer),&t->timer);
#ifdef CONDITIONAL_SEARCH
			else if(t->flags & PWM_ALERT)
				t->flags |= PWM_IS_ALERT;
			if (t->flags & PWM_IS_ALERT)
				max_seen = i;
#endif
		}
	}
#ifdef CONDITIONAL_SEARCH
	pwm_changed_cache = max_seen;
#endif
}

void init_pwm(void)
{
	pwm_t *t = pwms;
	port_t *p;
	uint8_t i;

	for(i=0;i<N_PWM;i++,t++) {
		p = &ports[t->port-1];
		t->flags &=~ PWM_IS_ON;
		// t->t_on = t->t_off = 0;
		port_set(p,0);
		if(t->t_off)
			timer_start(t->t_off, &t->timer);
	}
}


#endif // any PWM controllers
