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

/* This code implements basic adc input+output features.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>

#include "adc.h"
#include "features.h"
#include "moat.h"
#include "dev_data.h"
#include "debug.h"
#include "moat_internal.h"

#ifdef N_ADC

adc_t adcs[] = {
#include "_adc.h"
};

#ifndef HAVE_TIMER
#error Need HAVE_TIMER for ADC because I need to delay measuring
#endif

/* Each mainloop pass checks one adc. */
static uint8_t poll_this = 0;
static uint8_t poll_step = 0;
static uint8_t max_seen = 0;
/* Number of highest adc that has a change +1  */
uint8_t adc_changed_cache;

static inline char adc_check(adc_t *pp)
{
	uint8_t x;
	uint16_t val;
	switch(poll_step++) {
	case 0:
		if (pp->flags & ADC_REF)
			x = 0xE0;
		else
			x = 0x60;
		x |= (1<<ADLAR);
		if (!(pp->flags & ADC_ALT)) 
			x |= pp->flags&ADC_MASK;
		else switch(pp->flags & ADC_MASK) {
		case ADC_VBG:
			x |= 0x0E; break;
		case ADC_VGND:
			x |= 0x0F; break;
		case ADC_VTEMP:
			x |= 0x08; break;
		default:
			poll_step = 0;
			return 1;
		}
		ADMUX = x;
		ADCSRA = (1<<ADEN)|(1<<ADIF)|0x07; // slow
		break;
	case 1: break; // wait a bit
	case 2:
		ADCSRA |= (1<<ADSC);
		break;
	default:
		if(!(ADCSRA & (1<<ADIF)))
			break;
		val = ADCL;
		val |= ADCH<<8;
		val |= val>>10; // fill the lower bits, so that max=0xFFFF
		pp->value = val;
		if (pp->flags & ADC_ALERT) {
			if (pp->lower != 0xFFFF && pp->value <= pp->lower)
				pp->flags |= ADC_IS_ALERT_L;
			if (pp->upper != 0x0000 && pp->value >= pp->upper)
				pp->flags |= ADC_IS_ALERT_H;
		}
		poll_step = 0;
		return 1;
	}
	return 0;
}

void poll_adc(void)
{
	uint8_t i = poll_this;
	adc_t *pp;

	if (i >= N_ADC) {
		i=0;
		adc_changed_cache = max_seen;
		max_seen=0;
	}
	pp = &adcs[i];
	if (adc_check(pp)) {
		i += 1;
		if (pp->flags & (ADC_IS_ALERT_L|ADC_IS_ALERT_H))
			max_seen = i;
	}
	poll_this=i;
}

void init_adc(void)
{
	adc_t *pp = adcs;
	uint8_t i;

	for(i=0;i<N_ADC;i++) {
		pp->flags &=~(ADC_IS_ALERT_L|ADC_IS_ALERT_H);
		pp->lower = 0xFFFF;
		pp->upper = 0x0000;
		pp++;
	}
}


#endif // any adcs
