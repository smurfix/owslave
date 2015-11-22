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

#ifdef MAIN
#define EXTERN
#define INIT(x) = x
#else
#define EXTERN extern
#define INIT(x)
#endif

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
#define IMSK GIMSK
#define IFR GIFR
#endif

#if defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
#define F_CPU_                8000000
#define IMSK GIMSK
#define IFR GIFR
#define TIMSK0 TIMSK
#define TIFR0 TIFR

#define ADPIN PINB
#define ADPIN_vect PCINT0_vect
#define ADMSK PCMSK
#define PCIF0 PCIF
#define PCIE0 PCIE
#endif

#ifdef __AVR_ATmega8__
#define F_CPU_                8000000
#ifdef HAVE_DBG_PORT
#define DBGPORT PORTB
#define DBGDDR DDRB
#define DBGIN PINB
#endif
#ifdef HAVE_DBG_PIN
#define DBGPINPORT PORTD
#define DBGPIN PORTD4
#define DBGPINDDR DDRD
#define DBGPININ PIND
#endif

#define IMSK GIMSK
#define TIMSK0 TIMSK
#define TIFR0 TIFR
#define EEPE EEWE
#define EEMPE EEMWE
#define IFR GIFR
#endif

#ifdef __AVR_ATtiny84__
#define F_CPU_                8000000

#define IMSK GIMSK
#define IFR GIFR
#define ADPIN PINA
#define ADPIN_vect PCINT0_vect
#define ADMSK PCMSK0
#endif

#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega88__) || defined (__AVR_ATmega328__)
#define F_CPU_                16000000
#define ONEWIRE_USE_T2

#ifdef HAVE_DBG_PORT
#define DBGPORT PORTC
#define DBGDDR DDRC
#define DBGIN PINC
#endif
#ifdef HAVE_DBG_PIN
#define DBGPINPORT PORTD
#define DBGPIN PORTD4
#define DBGPINDDR DDRD
#define DBGPININ PIND
#endif

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

#ifndef ONEWIRE_USE_T2
# define ONEWIRE_USE_T0
#endif

#if defined(HAVE_UART_SYNC) && defined(HAVE_UART_IRQ)
#error Poll. Or IRQ. Not both.
#endif

#if defined(IS_BOOTLOADER) && defined(USE_BOOTLOADER)
#error "Either you're a the loader or you're the loaded."
#endif

#if defined(IS_BOOTLOADER) && !defined(USE_EEPROM)
#error "You need EEPROM support for bootloading"
#endif

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#endif /* FEATURES_H */
