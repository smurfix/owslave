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
#include "port.h"
#include "pwm.h"
#include "count.h"
#include "console.h"
#include "timer.h"
#include "crc.h"

#define _1W_READ_GENERIC  0xF2
#define _1W_WRITE_GENERIC 0xF4

#ifdef IS_BOOTLOADER
#include "moat_dummy.c"
#endif

#ifdef IS_BOOTLOADER
ALIASDEFS(loader)
static const moat_call_t dispatch_loader __attribute__ ((progmem)) = FUNCPTRS(loader);
#endif

void end_transmission(uint16_t crc)
{
	crc = ~crc;
	xmit_byte(crc);
	xmit_byte(crc >> 8);
	{
		uint16_t icrc;
		recv_byte();
		icrc = recv_byte_in();
		recv_byte();
		icrc |= recv_byte_in() << 8;
		if (icrc != ~crc) {
			DBG_P(" crc=");
			DBG_W(crc);
			DBG_P(" icrc=");
			DBG_W(icrc);
			DBG_C(' ');
			next_idle('c');
		}
		// DBG_P("CRC OK ");
	}
}

uint8_t moat_buf[MAXBUF];

#ifdef IS_BOOTLOADER
static const moat_call_t *dispatch;
static uint8_t tc_max;
#else
#define dispatch moat_calls
#define tc_max TC_MAX
#endif

// Inlining this code triggers a compiler bug
static void moat_read(void) __attribute__((noinline));
static void moat_read(void)
{
	uint16_t crc = 0;
	uint8_t dtype,chan;
	uint8_t len;
	uint8_t *bp=moat_buf;
	const moat_call_t *mc;
	read_len_fn *rlf;
	read_fn *rf;
	read_done_fn *rdf;

	/*
	 Implement reading data. We read the header, write the length,
	 write the data, write the CRC, read the inverted CRC back, and then do
	 whatever necessary to effect the read (e.g. remove the read data from
	 a buffer, clear a flag, whatever).
	 */
	
	recv_byte();
	crc = crc16(crc,_1W_READ_GENERIC);
	dtype = recv_byte_in();
	recv_byte();
	crc = crc16(crc,dtype);
	chan = recv_byte_in();
	//DBG_C('0'+dtype);
#ifdef IS_BOOTLOADER
	if (dtype == 0xFF) {
		mc = &dispatch_loader;
	} else
#endif
	{
		if (dtype >= tc_max)
			next_idle('p');
		mc = &dispatch[dtype];
	}

	rlf = pgm_read_ptr(&mc->read_len);
	len = rlf(chan);
	xmit_byte(len);

	crc = crc16(crc,chan);
	crc = crc16(crc,len);

	rf = pgm_read_ptr(&mc->read);
	rf(chan, moat_buf);

	while(len--) {
		xmit_byte(*bp);
		crc = crc16(crc,*bp);
		bp++;
	}
	end_transmission(crc);

	rdf = pgm_read_ptr(&mc->read_done);
	rdf(chan);
}

static void moat_write(void) __attribute__((noinline));
static void moat_write(void)
{
	uint16_t crc = 0;
	uint8_t dtype,chan;
	uint8_t len;
	const moat_call_t *mc;
	write_check_fn *wfc;
	write_fn *wf;

	/*
	 Write data. We read the header, read the length, read the data,
	 write the resulting CRC, read the inverted CRC,
	 and then do whatever necessary to effect the write (e.g. clear a flag,
	 update a stored value, whatever).
	 */
	
	recv_byte();
	crc = crc16(crc,_1W_WRITE_GENERIC);
	dtype = recv_byte_in();
	//DBG_C('W'); DBG_X(dtype);
	recv_byte();
	crc = crc16(crc,dtype);
	chan = recv_byte_in();
	recv_byte();
	crc = crc16(crc,chan);
	len = recv_byte_in();
	recv_byte();
	crc = crc16(crc,len);
	crc = recv_bytes_crc(crc, moat_buf, len);

#ifdef IS_BOOTLOADER
	if (dtype == 0xFF) {
		mc = &dispatch_loader;
	} else
#endif
	{
		if (dtype >= tc_max)
			next_idle('W');
		mc = &dispatch[dtype];
	}
	wfc = pgm_read_ptr(&mc->write_check);
	wfc(chan,moat_buf,len);
	end_transmission(crc);
	wf = pgm_read_ptr(&mc->write);
	wf(chan,moat_buf,len);
}

void moat_poll(void)
{
	static uint8_t i = 0;
	poll_fn *pf;

	if (i >= tc_max) {
		i = 0;
		return;
		// this makes sure that we do nothing if tc_max==0
	}
	pf = pgm_read_ptr(&dispatch[i].poll);
	pf();
	i++;
}

void moat_init(void)
{
	uint8_t i;
#ifdef IS_BOOTLOADER
	struct config_loader cl;
	const moat_loader_t *loader;
#endif
	const moat_call_t *mc;
	init_fn *pf;

#ifdef IS_BOOTLOADER
	loader = NULL;
	tc_max = 0;
	dispatch = NULL;

	if (!cfg_read (loader, cl)) {
		DBG_P("\nM:no cfg\n");
		return;
	}
	loader = (moat_loader_t *)cl.loader;
	if (! loader) {
		DBG_P("\nM:no loader\n");
		return;
	}
	if (pgm_read_byte(&loader->sig[0]) != 'M' ||
			pgm_read_byte(&loader->sig[1]) != 'L') {
		DBG_P("\nM:no sig ");
		DBG_W((uint16_t)loader);
		DBG_C('\n');
		return;
	}
	// TODO: calculate and check CRC

	tc_max = pgm_read_byte(&loader->n_types);
	dispatch = pgm_read_ptr(&loader->calls);

	DBG_P("\nM:has ");
	DBG_X(tc_max);
	DBG_C('\n');
	pf = pgm_read_ptr(&loader->init);
	pf();
#endif

	mc = dispatch;
	for(i=0;i<tc_max;i++,mc++) {
		pf = pgm_read_ptr(&mc->init);
		pf();
	}

	// Power reduction register allows us to turn off the clock to unused peripherals.
#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega88__) || defined (__AVR_ATmega328__)
   PRR =
			(1 << PRTWI) // TWI not used at all
        |(1 << PRSPI) // SPI not used at all
        |(1 << PRTIM1) // Timer 1 not used at all
			// Timer 2 is used for OW on Mega88
#ifndef HAVE_TIMER
        |(1 << PRTIM0)
#endif
#ifndef HAVE_UART
        |(1 << PRUSART0)
#endif
#if !defined(N_ADC)
        |(1 << PRADC)
#endif
		;
#elif defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)\
	|| defined (__AVR_ATtiny84__)
	PRR =
			 (1 << PRTIM1) // Timer 1 not used at all
			 // Timer 2 is used for OW on these devices
#ifndef HAVE_UART
			|(1 << PRUSI)
#endif
#if !defined(N_ADC)
			|(1 << PRADC)
#endif
		;

#endif
}

void do_command(uint8_t cmd)
{
	if(cmd == _1W_READ_GENERIC) {
		//DBG_P(":I");
		moat_read();
	} else if(cmd == _1W_WRITE_GENERIC) {
		//DBG_P(":I");
		moat_write();
	} else {
		DBG(0x0E);
		DBG_P("?CI ");
		DBG_X(cmd);
		set_idle();
	}
}

void update_idle(uint8_t bits)
{
	if(bits < 8)
		return;
	moat_poll();
}

#if CONSOLE_PING
timer_t t;
#endif
void init_state(void)
{
#if CONSOLE_PING
	timer_start(CONSOLE_PING,&t);
#endif
	moat_init();
}

#ifdef CONDITIONAL_SEARCH
uint8_t moat_alert_present;

uint8_t condition_met(void) {
	return moat_alert_present;

}
#endif

void mainloop(void) {
	DBG(0x1E);
	moat_poll();
#if CONSOLE_PING
	if(timer_done(&t)) {
		console_putc('!');
		timer_start(CONSOLE_PING,&t);
	}
#endif
}
