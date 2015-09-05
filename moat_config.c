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

/* This code implements the main code of MoaT slaves.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "pgm.h"
#include <string.h>

#include "onewire.h"
#include "features.h"
#include "moat.h"
#include "dev_data.h"
#include "debug.h"
#include "moat_internal.h"
#include "bug.h"

uint8_t read_config_len(uint8_t chan)
{
	cfg_addr_t off;
	uint8_t len;
	if (chan == CfgID_nums) {
		len = TC_MAX;
	} else if (chan == CfgID_list)  {
		len = (CFG_MAX+7)>>3;
	} else {
		off = cfg_addr(&len, chan);
		if (!off)
			next_idle('i');
	}
	return len;
}

void read_config(uint8_t chan, uint8_t *buf)
{
	cfg_addr_t off;
	uint8_t len;
	if (chan == CfgID_nums) {
		uint8_t i;
		for(i=0;i<TC_MAX;i++)
			*buf++ = pgm_read_byte(&moat_sizes[i]);
	} else if (chan == CfgID_list) {
		/* Return a bitmap of known types. Note that `b` may be zero for
		 * deleted blocks, but so is CfgID_list, thus that doesn't matter.
		 */
		uint8_t b;
		memset(buf,0,(CFG_MAX+7)>>3);
		BUILD_BUG_ON(CfgID_list != 0);
		BUILD_BUG_ON(CfgID_nums > 7);
		buf[0] = (1<<CfgID_list) | (1<<CfgID_nums);
		len = cfg_count(&off);
		while(len--) {
			b = cfg_type(&off);
			buf[b>>3] |= 1<<(b&7);
		}
	} else if (chan) {
		off = cfg_addr(&len, chan);
		if (!off)
			next_idle(('i'));
		while(len--)
			*buf++ = cfg_byte(off++);
	}
}

#ifdef USE_EEPROM
void write_config_check(uint8_t chan, uint8_t *buf, uint8_t len)
{
	if (chan == 0 || chan == CfgID_list || chan > CFG_MAX)
		next_idle('w');
}

void write_config(uint8_t chan, uint8_t *buf, uint8_t len)
{
	buf[len]=0;
	DBG_P("W_");
	DBG_X(chan);
	DBG_S((const char *)buf);
	if (_cfg_write(buf, len, chan))
		DBG_C('+');
	else
		DBG_C('-');
	DBG_C('\n');
}
#endif

