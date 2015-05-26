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
#include <avr/pgmspace.h>
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

#define _1W_READ_GENERIC  0xF2
#define _1W_WRITE_GENERIC 0xF4

const uint8_t moat_sizes[] __attribute__ ((progmem)) = {
#include "_nums.h"
};

void dummy_init_fn(void) {}
void dummy_poll_fn(void) {}
uint8_t dummy_read_len_fn(uint8_t chan) { next_idle('y'); return 0; }
void dummy_read_fn(uint8_t chan, uint8_t *buf) { next_idle('y'); }
void dummy_read_done_fn(uint8_t chan) {}
void dummy_write_fn(uint16_t crc) { next_idle('y'); }
char dummy_alert_check_fn(void) { return 0; }
void dummy_alert_fill_fn(uint8_t *buf) { next_idle('y'); }

#define TC_DEFINE(_s) \
    init_fn init_ ## _s __attribute__((weak,alias("dummy_init_fn"))); \
    poll_fn poll_ ## _s __attribute__((weak,alias("dummy_poll_fn"))); \
    read_len_fn read_ ## _s ## _len __attribute__((weak,alias("dummy_read_len_fn"))); \
    read_fn read_ ## _s __attribute__((weak,alias("dummy_read_fn"))); \
    read_done_fn read_ ## _s ## _done __attribute__((weak,alias("dummy_read_done_fn"))); \
    write_fn write_ ## _s __attribute__((weak,alias("dummy_write_fn")));  \
	ALERT_DEF(_s)
#ifdef CONDITIONAL_SEARCH
#define ALERT_DEF(_s) \
    alert_check_fn alert_ ## _s ## _check __attribute__((weak,alias("dummy_alert_check_fn"))); \
    alert_fill_fn alert_ ## _s ## _fill __attribute__((weak,alias("dummy_alert_fill_fn")));
#else
#define ALERT_DEF(x) // nothing
#endif
#include "_def.h"
#undef ALERT_DEF
#undef TC_DEFINE

#define TC_DEFINE(_s) \
{ \
    &init_ ## _s, \
    &poll_ ## _s, \
    &read_ ## _s ## _len, \
    &read_ ## _s, \
    &read_ ## _s ## _done, \
    &write_ ## _s, \
	ALERT_DEF(_s) \
},
#ifdef CONDITIONAL_SEARCH
#define ALERT_DEF(_s) \
    &alert_ ## _s ## _check, \
    &alert_ ## _s ## _fill, 
#else
#define ALERT_DEF(x) // nothing
#endif
const moat_call_t moat_calls[TC_MAX] __attribute__((progmem)) = {
#include "_def.h"
};
#undef ALERT_DEF
#undef TC_DEFINE

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

// Inlining this code triggers a compiler bug
static void moat_read(void) __attribute__((noinline));
static void moat_read(void)
{
	uint16_t crc = 0;
	uint8_t dtype,chan;
	static uint8_t buf[MAXBUF];
	uint8_t len;
	uint8_t *bp=buf;
	const moat_call_t *mc;
	read_len_fn *rlf;
	read_fn *rf;
	read_done_fn *rdf;

	/*
	 Implement reading data. We read whatever necessary, write the length,
	 write the data, write the CRC, read the inverted CRC back, and then do
	 whatever necessary to effect the read (e.g. clear a flag, update a
	 stored value, whatever).

	 Typically there's some free time between reading, writing the length,
	 and writing the data respectively. CRC read is immediate, so we
	 pre-calculate as much as possible and add the rest on the go.

	 Do not forget that bus errors and whatnot can abort this code in any
	 recv/xmit call.
	 */
	
	recv_byte();
	crc = crc16(crc,_1W_READ_GENERIC);
	dtype = recv_byte_in();
	recv_byte();
	crc = crc16(crc,dtype);
	chan = recv_byte_in();
	//DBG_C('0'+dtype);
	if (dtype >= TC_MAX)
		next_idle('p');
	mc = &moat_calls[dtype];

	rlf = pgm_read_ptr_near(&mc->read_len);
	len = rlf(chan);
	xmit_byte(len);

	crc = crc16(crc,chan);
	crc = crc16(crc,len);

	rf = pgm_read_ptr_near(&mc->read);
	rf(chan, buf);

	while(len--) {
		xmit_byte(*bp);
		crc = crc16(crc,*bp);
		bp++;
	}
	end_transmission(crc);

	rdf = pgm_read_ptr_near(&mc->read_done);
	rdf(chan);
}

static void moat_write(void) __attribute__((noinline));
static void moat_write(void)
{
	uint16_t crc = 0;
	uint8_t dtype;
	const moat_call_t *mc;
	write_fn *wf;

	/*
	 Implement reading data. We read whatever necessary, read the length,
	 read the data, write the resulting CRC, read the inverted CRC back,
	 and then do whatever necessary to effect the write (e.g. clear a flag,
	 update a stored value, whatever).

	 Do not forget that bus errors and whatnot can abort this code in any
	 recv/xmit call.
	 */
	
	recv_byte();
	crc = crc16(crc,_1W_WRITE_GENERIC);
	dtype = recv_byte_in();
	//DBG_C('W'); DBG_X(dtype);
	recv_byte();
	if (dtype >= TC_MAX)
		next_idle('W');
	crc = crc16(crc,dtype);

	mc = &moat_calls[dtype];
	wf = pgm_read_ptr_near(&mc->write);
	wf(crc);
}

void moat_poll(void)
{
	uint8_t i;
	const moat_call_t *mc = moat_calls;

	for(i=0;i<TC_MAX;i++,mc++) {
		poll_fn *pf = pgm_read_ptr_near(&mc->poll);
		pf();
	}
}

void moat_init(void)
{
	uint8_t i;
	const moat_call_t *mc = moat_calls;

	for(i=0;i<TC_MAX;i++,mc++) {
		init_fn *pf = pgm_read_ptr_near(&mc->init);
		pf();
	}
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
extern uint8_t alert_present;

uint8_t condition_met(void) {
	return alert_present;

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
