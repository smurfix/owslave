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

#ifndef FEATURES_H
#define FEATURES_H

#include <avr/io.h>
#include <avr/interrupt.h>

#include "dev_config.h"

#ifndef BAUDRATE
#define BAUDRATE 38400
#endif

/* External definitions:
 * F_CPU       clock rate
 * SKIP_SEARCH if you don't have enough ROM for search (saves ~200 bytes)
 * HAVE_UART   if you want to debug your code
 */
#ifdef __AVR_ATtiny13__
#define F_CPU_                9600000
#define OWPIN PINB
#define OWPORT PORTB
#define OWDDR DDRB
#define ONEWIREPIN 1		 // INT0

#define IMSK GIMSK
#define IFR GIFR
#endif

#ifdef __AVR_ATtiny25__
#define F_CPU_                8000000
#define OWPIN PINB
#define OWPORT PORTB
#define OWDDR DDRB
#define ONEWIREPIN 1		 // INT0

#define IMSK GIMSK
#define IFR GIFR

#define ADPIN PINB
#define ADPIN_vect PCINT0_vect
#define ADMSK PCMSK
#define PCIF0 PCIF
#define PCIE0 PCIE
#endif

#ifdef __AVR_ATmega8__
#define F_CPU_                8000000
#define OWPIN PIND
#define OWPORT PORTD
#define OWDDR DDRD
#define DBGPORT PORTB
#define DBGDDR PINB
#define ONEWIREPIN 2		// INT0

#define IMSK GIMSK
#define TIMSK0 TIMSK
#define TIFR0 TIFR
#define EEPE EEWE
#define EEMPE EEMWE
#define IFR EIFR
#endif

#ifdef __AVR_ATtiny84__
#define F_CPU_                8000000
#define OWPIN PINB
#define OWPORT PORTB
#define OWDDR DDRB
#define ONEWIREPIN 2		 // INT0

#define IMSK GIMSK
#define IFR GIFR
#define ADPIN PINA
#define ADPIN_vect PCINT0_vect
#define ADMSK PCMSK0
#endif

#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega88__)
#define F_CPU_                16000000
#define OWPIN PIND
#define OWPORT PORTD
#define OWDDR DDRD
#define ONEWIREPIN 2		// INT0

#define DBGPORT PORTB
#define DBGDDR DDRB
#define DBGIN PINB

#define IMSK EIMSK
#define IFR EIFR
#define ADPIN PINC
#define ADPIN_vect PCINT1_vect
#define ADMSK PCMSK1
#define ADIRQ

#endif

#ifndef F_CPU_
#error Unknown AVR chip!
#endif

#ifndef F_CPU
#define F_CPU F_CPU_
#endif

#if defined(HAVE_UART_SYNC) && defined(HAVE_UART_IRQ)
#error Poll. Or IRQ. Not both.
#endif

#endif /* FEATURES_H */
