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

/* This code implements reading the console buffer via 1wire.
 */

#include <avr/boot.h>
#include "pgm.h"

#include "moat_internal.h"
#include "dev_data.h"
#include "debug.h"
#include "onewire.h"
#include "console.h"

#ifdef IS_BOOTLOADER

#define FIRST (LOAD_START/SPM_PAGESIZE)

extern void boot_program_page (uint32_t page, uint8_t *buf);
extern char _etext;

uint8_t read_loader_len(uint8_t chan)
{
	if(chan == 0)
		return 3;
	else if (chan <= CFG_MAX) {
        uint8_t len;
        cfg_addr_t off = cfg_addr(&len, chan);
		if (!off)
			next_idle('i');
        
        return len;
	} else
		return SPM_PAGESIZE;
}

void read_loader(uint8_t chan, uint8_t *buf)
{
	if (chan == 0) {
		*buf++ = SPM_PAGESIZE;
		*buf++ = (((uint16_t)&_etext) + (SPM_PAGESIZE-1)) / SPM_PAGESIZE;
		*buf++ = ((uint16_t)&boot_program_page) / SPM_PAGESIZE;
	}
	else if (chan <= CFG_MAX) {
		uint8_t len;
		cfg_addr_t off = cfg_addr(&len, chan);

		if (!off)
			next_idle(('i'));
		while(len--)
			*buf++ = cfg_byte(off++);
	} else {
		uint8_t len = SPM_PAGESIZE;
		uint16_t adr = chan * SPM_PAGESIZE;
		while(len--)
			*buf++ = pgm_read_byte(adr++);
	}
}

void write_loader_check(uint8_t chan, uint8_t *buf, uint8_t len)
{
	if (chan == 0 || chan == CfgID_list)
		next_idle('w');
	if (chan >= CFG_MAX) {
		if (chan*SPM_PAGESIZE < (uint16_t)&_etext)
			next_idle('f');
		if ((chan+1)*SPM_PAGESIZE >= (uint16_t)&boot_program_page)
			next_idle('g');
		if (len != SPM_PAGESIZE)
			next_idle('h');
	}
}

void write_loader(uint8_t chan, uint8_t *buf, uint8_t len)
{
	if (chan < CFG_MAX)
		_cfg_write(buf, len, chan);
	else
		boot_program_page (chan*SPM_PAGESIZE, buf);
}

#endif
