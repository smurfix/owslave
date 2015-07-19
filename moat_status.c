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
#include "status.h"
#include "timer.h"
#include "_status.h"

#ifdef N_STATUS

#if N_STATUS>1 && defined(IS_BOOTLOADER)
static const char buildv[] __attribute__ ((progmem)) = BUILDVER;
#endif

uint8_t read_status_len(uint8_t chan)
{
	switch (chan) {
	case 0:
		return 1;
	case S_reboot:
		return 1;
#if N_STATUS>1 && defined(IS_BOOTLOADER)
	case S_loader:
		return strlen(BUILDVER);
#endif
	default:
		next_idle('s');
	}
		
}

void read_status(uint8_t chan, uint8_t *buf)
{
	switch(chan) {
	case 0:
		*buf = ((1<<(STATUS_MAX-1))-1)
#if N_STATUS < 2 || !defined(IS_BOOTLOADER)
			& ~(1<<(S_loader-1))
#endif
		;
		break;
	case S_reboot:
		*buf = status_boot;
#ifdef CONDITIONAL_SEARCH
		init_msg &=~ (1<<(S_reboot-1));
#endif
		break;
#if N_STATUS>1 && defined(WITH_BOOTLOADER)
	case S_loader:
		const char *v = buildv;
		while ((*buf = pgm_read_byte(v))) {
			buf++;
			v++;
		}
		break;
#endif
	default:
		next_idle('s');
	}
}

#ifdef CONDITIONAL_SEARCH

char alert_status_check(void)
{
	if (init_msg)
		return 1;
	return 0;
}

void alert_status_fill(uint8_t *buf)
{
	memset(buf,0,(STATUS_MAX+7)>>3);
	*buf = init_msg;
}

#endif // conditional


#endif // N_STATUS
