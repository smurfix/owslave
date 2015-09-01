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

/* This code implements reading alert pins via 1wire.
 */

#include "pgm.h"
#include <string.h> // memset

#include "moat_internal.h"
#include "dev_data.h"
#include "debug.h"
#include "onewire.h"

#ifdef CONDITIONAL_SEARCH

#ifndef N_ALERT
#error You need to set alert=1
#endif
#if N_ALERT != TC_MAX
#error Something is wrong in your configuration
#endif

uint8_t alert_buf[(TC_MAX+7)>>3];
static uint8_t alert_tmp[(TC_MAX+7)>>3];
uint8_t alert_pos=0;
static uint8_t alert_max;

void poll_alert(void)
{
	uint8_t chan = alert_pos;
	const moat_call_t *mc;
	alert_check_fn *ac;

	if(chan >= TC_MAX) {
		chan=0;
		memcpy(alert_buf,alert_tmp,sizeof(alert_buf));
		memset(alert_tmp,0,sizeof(alert_tmp));
		moat_alert_present = alert_max;
		alert_max = 0;
	}
	mc = &moat_calls[chan];
	ac = pgm_read_ptr(&mc->alert_check);
	if(ac()) {
		alert_tmp[chan>>3] |= 1<<(chan&7);
		alert_max = chan+1;
	}
	alert_pos = chan+1;
}

uint8_t read_alert_len(uint8_t chan)
{
	uint8_t len;
	if(!chan)
		len = (moat_alert_present+7)>>3;
	else if (chan >= TC_MAX)
		next_idle('x');
	else
		len = pgm_read_byte(&moat_sizes[chan]);
	return (len+7)>>3;
}

void read_alert(uint8_t chan, uint8_t *buf)
{
	if (chan) { // fill alert bitmap
		alert_fill_fn *ac;
		const moat_call_t *mc;
		if (chan >= TC_MAX)
			next_idle('p');
		mc = &moat_calls[chan];
		ac = pgm_read_ptr(&mc->alert_fill);

		ac(buf);
	} else { // all inputs: send bits
		memcpy(buf,alert_buf,sizeof(alert_buf));
	}
}

#endif
