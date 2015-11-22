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

/* This code implements basic n-10th-of-a-second timers.
 * 
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "pgm.h"
#include <string.h>

#include "timer.h"
#include "features.h"
#include "moat.h"
#include "dev_data.h"
#include "debug.h"
#include "moat_internal.h"

#ifdef HAVE_TIMER

#define CLOCKS 125 // timer0 is 8-bit, so <=255
#define PRESCALE 256
#define SUB (F_CPU/10/PRESCALE/CLOCKS) // clocks per msec
// 10: accuracy (10th of a second)
// PRESCALE: prescaler
// CLOCKS: timeout
// This results in a nice integer value for 8/16 MHz

// For 20MHz, alternating between N and N+1 results in an error of .0064% instead of 0.8%
// good enough, given the xtal precision
#if (F_CPU == 20000000) && (PRESCALE == 256)
#define SUB2 (SUB+SUB+1)
#else
#define SUB2 0
#endif

static int16_t current = 0;
static uint8_t sub = 0;
#if SUB2
static uint8_t nsub = SUB;
#endif

/* return True every MS milliseconds */
char timer_done(timer_t *t)
{
	if((current - t->last) >= 0)
		return 1;
	return 0;
}

int16_t timer_remaining(timer_t *t)
{
	return t->last-current;
}

void timer_start(int16_t sec, timer_t *t)
{
	t->last += sec;
}

void timer_reset(timer_t *t)
{
	t->last = current;
}

void timer_init(void)
{
#if defined (__AVR_ATmega8__)
#define TCCR0B TCCR0
#else
	TCCR0A=0;
#endif
#if PRESCALE==64
	TCCR0B=0x03;
#elif PRESCALE==256
	TCCR0B=0x04;
#elif PRESCALE==1024
	TCCR0B=0x05;
#else
#error Wrong value of PRESCALE!
#endif
	TCNT0=~CLOCKS;
	TIMSK0=(1<<TOIE0);
	TIFR0=(1<<TOV0);
}

void timer_poll(void)
{
	/* Actually handled in interrupt. */
}

ISR(TIMER0_OVF_vect)
{
	TCNT0=~CLOCKS;
	if(!--sub) {
		current += 1;
#if SUB2
		sub = nsub;
		nsub = SUB2-nsub;
#else
		sub = SUB;
#endif
	}
}

#endif // timer_h
