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

/* This code implements reading adc pins via 1wire.
 */

#include <string.h>

#include "moat_internal.h"
#include "dev_data.h"
#include "debug.h"
#include "onewire.h"
#include "adc.h"

#ifdef N_ADC

uint8_t read_adc_len(uint8_t chan)
{
	if(chan)
		return 7;
	else
		return N_ADC*2;
}

void read_adc(uint8_t chan, uint8_t *buf)
{
	adc_t *adcp;
	if (chan) { // one input: send flags, mark as scanned
		if (chan > N_ADC)
			next_idle('p');
		adcp = &adcs[chan-1];
		*buf++ = adcp->flags;
		*buf++ = adcp->value>>8;
		*buf++ = adcp->value&0xFF;
		*buf++ = adcp->lower>>8;
		*buf++ = adcp->lower&0xFF;
		*buf++ = adcp->upper>>8;
		*buf++ = adcp->upper&0xFF;
	} else { // all inputs: send bits
		uint8_t i;
		adcp = adcs;

		for(i=0;i<N_ADC;i++) {
			*buf++ = adcp->value>>8;
			*buf++ = adcp->value&0xFF;
			adcp++;
		}
	}
}

void read_adc_done(uint8_t chan) {
	adc_t *adcp;
	if(!chan) return;
	adcp = &adcs[chan-1];
	adcp->flags &=~ (ADC_IS_ALERT_L|ADC_IS_ALERT_H);
}

void write_adc(uint16_t crc)
{
	uint8_t chan;
	uint8_t len, x;
	uint16_t lower,upper;
	adc_t *adcp;
	chan = recv_byte_in();
	recv_byte();

	if (chan == 0 || chan > N_ADC)
		next_idle('p');
	adcp = &adcs[chan-1];

	crc = crc16(crc,chan);

	len = recv_byte_in();
	recv_byte();
	if (len != 2 && len != 4)
		next_idle('a');
	crc = crc16(crc,len);

	x = recv_byte_in();
	recv_byte();
	crc = crc16(crc,x);
	if (len == 2)
		lower = (x<<8)|x;
	else
		lower = x<<8;

	x = recv_byte_in();
	if (len == 2) {
		crc = crc16(crc,x);
		upper = (x<<8)|x;
	} else {
		recv_byte();
		crc = crc16(crc,x);
		lower |= x;

		x = recv_byte_in();
		recv_byte();
		crc = crc16(crc,x);
		upper = (x<<8);
		x = recv_byte_in();
		crc = crc16(crc,x);
		upper |= x;
	}
	end_transmission(crc);

	adcp->lower = lower;
	adcp->upper = upper;
}

#ifdef CONDITIONAL_SEARCH

char alert_adc_check(void)
{
	return (adc_changed_cache*2 +7)>>3;
}

void alert_adc_fill(uint8_t *buf)
{
	uint8_t i;
	adc_t *t = adcs;
	uint8_t m=1;

    DBG_X(adc_changed_cache);
	memset(buf,0,(N_ADC*2 +7)>>3);
	for(i=0;i < N_ADC; i++,t++) {
		if (!m) {
			m = 1;
			buf++;
		}
		if (t->flags & ADC_IS_ALERT_L)
			*buf |= m;
		m <<= 1;
		if (t->flags & ADC_IS_ALERT_H)
			*buf |= m;
		m <<= 1;
	}
}

#endif // conditional


#endif
