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

/* This code implements counting transitions.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>

#include "port.h"
#include "count.h"
#include "features.h"
#include "moat.h"
#include "dev_data.h"
#include "debug.h"
#include "timer.h"
#include "moat_internal.h"

#ifdef N_COUNT

count_t counts[] = {
#include "_count.h"
};

static uint8_t poll_next = 0;
static uint8_t max_seen = 0;
void count_poll(void)
{
	uint8_t i = poll_next;
	count_t *t;
	port_t *p;

	if (i >= N_COUNT)
		i = 0;
	if (!i) {
		count_changed_cache = max_seen;
		max_seen = 0;
	}
	t = &counts[i];
	i++;
	poll_next = i;

	p = &ports[t->port-1];
	if(!(p->flags & PFLG_CURRENT) == !(t->flags & CF_IS_ON))
		return;
	if (p->flags & PFLG_CURRENT)
		t->flags |= CF_IS_ON;
	else
		t->flags &=~CF_IS_ON;
	t->count++;
	if(t->flags & CF_ALERTING) {
		t->flags |= CF_IS_ALERT;
		max_seen = i;
	}
	poll_next = i;
}

void count_init(void)
{
	count_t *t = counts;
	uint8_t i;

	for(i=0;i<N_COUNT;i++,t++) {
		t->count = 0;
	}
}


#endif // any COUNT controllers
