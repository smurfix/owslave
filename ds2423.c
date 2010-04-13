/*
 *  Copyright © 2010, Matthias Urlichs <matthias@urlichs.de>
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

/* This code implements the DS2423 counter (obsolete and no-longer-produced).
 * Only the features necessary for reading the physical counters are
 * implemented; the rest is of no particular interest. Yet.
 */

/* Input pins are PA0 thru PA7, depending on the definition of NCOUNTERS.
   If ANALOG is defined, these pins are read by the ADC and an adaptive
   hysteresis is used to trigger the counters, otherwise they are used
   as straight digital inputs. */

/* if SLOW is defined, a decaying average (factor 2^-SLOW) is used to
   lowpass-filter the (analog) result. Use this if you want to monitor e.g.
   a blinking LED, while not being distracted by ambient neon lighting.
   Note that sample frequency is approx. 8 kHz / NCOUNTERS.
 */

#ifndef NCOUNTERS
#define NCOUNTERS 2
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include "onewire.h"
#include "features.h"

#define C_WRITE_SCRATCHPAD 0x0F // TODO
#define C_READ_SCRATCHPAD  0xAA // TODO
#define C_COPY_SCRATCHPAD  0x55 // TODO
#define C_READ_MEM         0xF0 // TODO
#define C_READ_MEM_COUNTER 0xA5 // TODO

#ifdef ANALOG
// Minimal hysteresis between hi and lo states
#define HYST 100 // initial hysteresis; approx. 500 mV
// At 8 KHz sampling rate (approx), 1/10th second should be enough 
#define SLOW 4 // decay filter for lowpass-filtering

static uint16_t last[NCOUNTERS];
static uint16_t hyst[NCOUNTERS];
#ifdef SLOW
#if SLOW > 5
static uint32_t decay[NCOUNTERS];
#else
static uint16_t decay[NCOUNTERS];
#endif
#endif
static uint8_t cur_adc,bstate;
static uint16_t samples;

#else // !ANALOG
static uint8_t obits,cbits;
#endif
static uint32_t counter[NCOUNTERS];
static uint8_t unchecked;

void do_mem_counter(void)
{
	uint16_t crc = 0;
	uint8_t b;
	uint8_t len;
	uint16_t adr;

	/*
	 The following code does:
         * receive address (2 bytes), add them to CRC
         * send 1..32 bytes (0xFF), add them to CRC
	 * send counter (4 bytes, CRC)
	 * send 4 zero bytes
	 * send inverted CRC
         This is all very straightforward, except that the CRC calculation
	 is delayed somewhat: the time between recv_byte_in() and xmit_byte()
	 is only a bit wide, which may not be enough time to add a CRC byte.
	 */
	
	recv_byte();
	crc = crc16(crc,C_READ_MEM_COUNTER);
	b = recv_byte_in();
	recv_byte();
	crc = crc16(crc,b);
	adr = b;
	b = recv_byte_in();
	adr |= b<<8;
	if(1) {
		xmit_byte(0xFF);
		crc = crc16(crc,b);
	} else {
xmore:
		xmit_byte(0xFF);
	}
	len = 0x1F - (adr & 0x1F);
	adr++;
	crc = crc16(crc,0xFF);

	while(len) {
		xmit_byte(0xFF);
		crc = crc16(crc,0xFF);
		adr++;
		len--;
	}
#define SEND(_x) do {                                        \
		uint32_t x;                                  \
		cli();                                       \
		x = (_x);                                    \
		sei();                                       \
		b = x    ; xmit_byte(b); crc = crc16(crc,b); \
		b = x>> 8; xmit_byte(b); crc = crc16(crc,b); \
		b = x>>16; xmit_byte(b); crc = crc16(crc,b); \
		b = x>>24; xmit_byte(b); crc = crc16(crc,b); \
		} while(0)
	
	b = (0x10 - (adr>>5));
	if (b < NCOUNTERS)
		SEND(counter[b]);
#if defined(ANALOG) && NCOUNTERS == 1
	else if(b == 1) // quick&dirty debugging
		SEND(last[0]);
#endif
	else {
		DBG_C('@');
		DBG_Y(adr);
		SEND(0xFFFFFFFF);
	}
	SEND(0);
#undef SEND

	crc = ~crc;
	xmit_byte(crc);
	xmit_byte(crc >> 8);
	if(adr & 0x1FF) {
		crc = 0;
		goto xmore;
	}
	while(1)
		xmit_bit(1);
}

void do_command(uint8_t cmd)
{
	if(cmd == C_READ_MEM_COUNTER) {
		DBG_P(":I");
		do_mem_counter();
	} else {
		DBG_P("?CI");
		DBG_X(cmd);
		set_idle();
	}
}

#ifdef ANALOG
void start_adc(void)
{
	ADMUX = cur_adc;
	ADCSRA |= (1<<ADSC);
}
#endif

void check_adc(void)
{
#ifdef ANALOG
	uint16_t res;
	uint8_t cur;
	if(!(ADCSRA & (1<<ADIF)))
		return;
	res = ADC >> 1;
	cur = cur_adc;
	if(cur_adc)
		cur_adc--;
	else
		cur_adc = NCOUNTERS-1;
	start_adc();

// Decaying average. To avoid losing precision, use 32 bits if SLOW>5.
// (GCC generates spectacularly-inefficient code for this.)
#ifdef SLOW
	{
#if SLOW > 5
		uint32_t last = decay[cur];
		uint32_t ires = res << (16-SLOW);
		ires += last - (last>>SLOW);
		decay[cur] = ires;
		res = ires>>16;
#else
		uint16_t last = decay[cur];
		res = (res>>SLOW) + last - (last>>SLOW);
		decay[cur] = res;
#endif
	}
#endif
	if(!(bstate&(1<<cur))) {
		if (res < last[cur])
			last[cur] = res;
		else if (res > hyst[cur]+last[cur]) {
			bstate |= (1<<cur);
			if(samples)
				counter[cur]++;
			last[cur] = res;
		}
	} else {
		if (res > last[cur])
			last[cur] = res;
		else if(res+hyst[cur] < last[cur]) {
			bstate &= (1<<cur);
			last[cur] = res;
		}
	}
	if(samples < 0xFFFF)
		samples++;
#else
	uint8_t i = 0;
	uint8_t now_bits,nbits,bits,ocbits;
	cli();
	now_bits = ADPIN;
	nbits = now_bits;
	bits = cbits;
	ocbits = obits;
	cbits = 0;
	sei();
	while(i < NCOUNTERS) {
		// Count a 0-1 transition. We may have missed the subsequent 1-0.
		if ((bits & 0x01) && ((nbits & 0x01) || !(ocbits & 0x01)))
			counter[i]++;
		ocbits >>= 1;
		nbits >>= 1;
		bits >>= 1;
		i++;
	}
	obits = now_bits;
#endif
	unchecked = 0;
}

#ifndef ANALOG
ISR(ADPIN_vect)
{
	uint8_t nbits = ADPIN;
	nbits ^= obits; // 'nbits' now contains the changed bits
	cbits |= nbits;
}
#endif

void update_idle(uint8_t bits)
{
	//DBG_C('\\');
	if(bits > 0 || unchecked > 100)
		check_adc();
	else if((ADCSRA & (1<<ADIF)) && (unchecked < 0xFF))
		unchecked++;

#ifdef ANALOG
	if (bits < NCOUNTERS)
		return;
#if 0
	// ten times per second or so, do some housekeeping
	if (samples > 800/NCOUNTERS) {
		uint8_t i = NCOUNTERS;
		samples -= 800/NCOUNTERS;

		while(i) {
			i--;
			// TODO
		}

	}
#endif
#endif
}

void init_state(void)
{
#ifdef ANALOG
	uint8_t i;
#endif
	memset(counter,0,sizeof(counter));
#ifdef ANALOG
#ifdef SLOW
	memset(decay,0,sizeof(decay));
#endif
	for(i = NCOUNTERS; i; ) {
		i--;
		hyst[i] = HYST<<5;
	}

	ADMUX = 0;
	DIDR0 = (1<<NCOUNTERS)-1;

#if F_CPU >= 12800000 // prescale AD clock to <= 200 KHz
#define CLK_A 7
#elif F_CPU >= 6400000
#define CLK_A 6
#else
#define CLK_A 5
#endif
	ADCSRA = (1<<ADEN)|(1<<ADIF)|CLK_A;
	ADCSRB |= (1<<ADLAR);

	cur_adc = 0;
	bstate = 0;
	start_adc();
#else
	obits = ADPIN;
	ADMSK = (1<<NCOUNTERS)-1;
	IFR |= (1<<PCIF0);
	IMSK |= (1<<PCIE0);
#endif
}

