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

/* This code implements basic port input+output features.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "pgm.h"
#include <string.h>

#include "port.h"
#include "features.h"
#include "moat.h"
#include "dev_data.h"
#include "debug.h"
#include "moat_internal.h"

#ifdef N_PORT

port_t ports[] = {
#include "_port.h"
};

void port_set(port_t *portp, char val)
{
	/* We could pack each switch{} into a single 8-bit value */
	uint8_t flg = portp->flags;
	port_out_t s = port_get_out(portp);

	// Rather than write a complicated switch statement, we go by bits.
	if(flg & PFLG_ALT) {
		// This does PO_OFF=0 | PO_PULLUP=3 and PO_ON=1 | PO_Z=2
		// OFF/ON must be 0/1, so test bit 0
		if((s & 1) != !val) return;
		s ^= 3;
	} else if(flg & PFLG_ALT2) {
		// This does PO_OFF=0 | PO_Z=2 and PO_ON=1 | PO_PULLUP=3
		// OFF/ON must be 0/1, so bit 1 is tested+switched while bit 0 inverts the test if it's set
		if ((1&((s>>1) ^ s)) != !val) return;
		s ^= 2;
	} else {
		// This does PO_OFF=0 | PO_ON=1 and PO_Z=2 | PO_PULLUP=3
		if((s & 1) != !val) return;
		s ^= 1;
	}
	port_set_out(portp,s);

	// Set "current" to the expected state which we just set.
	// The next poll will notice if that doesn't match the actual port.
	if (val)
		flg |= PFLG_CURRENT;
	else
		flg &=~PFLG_CURRENT;
	portp->flags = flg;
}

/*
 * The idea is to clear the POLL flag if the port has not changed since
 * reporting started. Otherwise, check again.
 *
 * The HAS_CHANGED macro sets the POLL flag and clears CHANGED when it sees
 * CHANGED set.
 */

void port_check(port_t *pp) {
	_P_VARS(pp)
	char s = _P_GET(pin);
	if (s != !!(flg & PFLG_CURRENT)) {
		// DBG_C('P');DBG_N(pp-ports);DBG_C('=');DBG_N(s); DBG_C(' ');
		if(s)
			flg |= PFLG_CURRENT;
		else
			flg &=~PFLG_CURRENT;
		pp->flags = flg | PFLG_CHANGED;
	}
}

/* Each mainloop pass checks one port. */
static uint8_t poll_next = 0;
#ifdef CONDITIONAL_SEARCH
uint8_t port_changed_cache;
static uint8_t max_seen = 0;
#endif
void poll_port(void)
{
	port_t *pp;
	uint8_t i = poll_next;
	if (i >= N_PORT) {
		i=0;
#ifdef CONDITIONAL_SEARCH
		port_changed_cache = max_seen;
		max_seen=0;
#endif
	}
	pp = &ports[i];
	i++;
	port_check(pp);
#ifdef CONDITIONAL_SEARCH
	if(pp->flags & (PFLG_POLL|PFLG_CHANGED) && pp->flags & PFLG_ALERT)
		max_seen = i;
#endif
	poll_next=i;
}

void init_port(void)
{
	port_t *pp = ports;
	uint8_t i;

	for(i=0;i<N_PORT;i++) {
		_P_VARS(pp)
		port_set_out(pp,flg);

		if (_P_GET(pin))
			flg |= PFLG_CURRENT;
		else
			flg &=~PFLG_CURRENT;
		pp->flags = flg &~PFLG_CHANGED;
		pp++;
	}
#ifdef CONDITIONAL_SEARCH
	port_changed_cache = 0;
#endif
}


#endif // any ports
