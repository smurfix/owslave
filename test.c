/*
 *  Copyright © 2010-2015, Matthias Urlichs <matthias@urlichs.de>
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

/* This code implements a smple test code which uses a delay loop to toggle
 * a pin and emit a serial character.
 */

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <string.h>
#include "onewire.h"
#include "features.h"
#include "debug.h"

#ifndef TIMSK
#define TIMSK TIMSK0
#endif
#ifndef TIFR
#define TIFR TIFR0
#endif
#ifndef EICRA
#define EICRA MCUCR
#endif

#define EN_TIMER() do {TIFR|=(1<<TOV0); TIMSK|=(1<<TOIE0);}while(0) //enable timer interrupt
#define DIS_TIMER() do {TIMSK &= ~(1<<TOIE0);} while(0) // disable timer interrupt

static unsigned long long x = 0;
void init_state(void) {
	wdt_reset();
    DBG(0x01);
    DBG_ON();
    DBG_OFF();
    DBG_ON();

    //TCCR0A = 0;
    //TCCR0B = 0x03;  // Prescaler 1/64

    DBG_OFF();
    DBG_ON();

    //EN_TIMER();
    //TCNT0=0xF0;

    DBG_OFF();
    DBG(0x03);
}

void mainloop(void) {
    DBG(0x02);
    DBG_ON();
    DBG_OFF();
	wdt_reset();
    if(++x<100000ULL) {
        DBG(0x06);
        return;
    }
    DBG(0x0A);
    x = 0;
    DBG_C('/');
    //TCNT0=0xE0;
    //EN_TIMER();
    DBG(0x0B);
}
