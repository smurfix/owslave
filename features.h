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

#ifndef FEATURES_H
#define FEATURES_H

/*! there are 3 types of configuration settings:
 *  - uP specific settings (like uP type, register names etc.)
 *  - hardware specific settings (like fuse stuff: F_CPU, pin usage ...)
 *  - project specific settings (ds2408, ds2423, ...)
 */

/* hardware specific settings, just as the cpu should come from
 * either the makefile or better something like Kconfig
 */
#ifndef F_CPU
#warning "CPU frequency not defined, assuming 8MHz"
#define F_CPU	8000000
#endif

/* other project specific settings
 * SKIP_SEARCH if you don't have enough ROM for search (saves ~200 bytes)
 * HAVE_UART   if you want to debug your code
*/


#ifdef __AVR__
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define OW_PINCHANGE_ISR() ISR (INT0_vect)
#define OW_TIMER_ISR() ISR (TIMER0_OVF_vect)

// stupidity
#ifndef TIMER0_OVF_vect
#  define TIMER0_OVF_vect TIM0_OVF_vect
#endif

// for atmega8 and atmega32
#ifndef EEPE
#define EEPE EEWE
#endif
#ifndef EEMPE
#define EEMPE EEMWE
#endif

// this works for all AVRs, getting address from address 0..7
static inline void get_ow_address(uint8_t *addr)
{
	uint8_t i;

	 // Wait for EPROM circuitry to be ready
	while(EECR & (1<<EEPE));

	for (i=8; i;) {
		i--;
		EEARL = 7-i;			// set EPROM Address
		EECR |= (1<<EERE);		// Start eeprom read by writing EERE
		addr[i] =  EEDR;		// Return data from data register
	}
}
#endif

/* define __CPU used as name prefix and
 * their uP setup functions, they:
 * - define appropriate clock settings
 * - define the prescaler used the OW_timer
 * - setup the OW_pinchange interrupt
 */
#if defined(__AVR_ATtiny13__)
#define __CPU	AVR_ATtiny13

static inline void AVR_ATtiny13_setup(void)
{
	CLKPR = 0x80;	// Prepare to ...
	CLKPR = 0x00;	// ... set to 9.6 MHz
	TCCR0A = 0;
	TCCR0B = 0x03;	// Prescaler 1/64
	MCUCR |= (1 << ISC00);	// Interrupt on both level changes
}

static inline void AVR_ATtiny13_mask_owpin(void) { GIMSK &= ~(1 << INT0); }
static inline void AVR_ATtiny13_unmask_owpin(void) { GIFR |= (1 << INTF0); GIMSK |= (1 << INT0); }
static inline void AVR_ATtiny13_set_owtimer((uint8_t timeout)
{
	TCNT0 = ~timeout;	// overrun at 0xFF
	TIFR0 |= (1 << TOV0);
	TIMSK0 |= (1 << TOIE0);
}
static inline void AVR_ATtiny13_clear_owtimer(void) { TCNT0 = 0; TIMSK0 &= ~(1 << TOIE0); }

// use INT0 pin (PORT B1)
static inline void AVR_ATtiny13_owpin_setup(void) { PORTB &= ~2; DDRB &= ~2; }
static inline void AVR_ATtiny13_owpin_low(void) { DDRB |= 2; }
static inline void AVR_ATtiny13_owpin_hiz(void) { DDRB &= ~2; }
static inline uint8_t AVR_ATtiny13_owpin_value(void) { return PINB & 2; }

#elif defined (__AVR_ATmega8__)
#define __CPU	AVR_ATmega8

static inline void AVR_ATmega8_setup(void)
{
	// Clock is set via fuse, at least to 8MHz
	TCCR0 = 0x03;			// Prescaler 1/64
	MCUCR |= (1 << ISC00);	// Interrupt on both level changes
}

static inline void AVR_ATmega8_mask_owpin(void) { GIMSK &= ~(1 << INT0); }
static inline void AVR_ATmega8_unmask_owpin(void) { GIFR |= (1 << INTF0); GIMSK |= (1 << INT0); }
static inline void AVR_ATmega8_set_owtimer(uint8_t timeout)
{
	TCNT0 = ~timeout;	// overrun at 0xFF
	TIFR |= (1 << TOV0);
	TIMSK |= (1 << TOIE0);
}
static inline void AVR_ATmega8_clear_owtimer(void) { TCNT0 = 0; TIMSK &= ~(1 << TOIE0); }

// use INT0 pin (PORT B2)
static inline void AVR_ATmega8_owpin_setup(void) { PORTB &= ~4; DDRB &= ~4; }
static inline void AVR_ATmega8_owpin_low(void) { DDRB |= 4; }
static inline void AVR_ATmega8_owpin_hiz(void) { DDRB &= ~4; }
static inline uint8_t AVR_ATmega8_owpin_value(void) { return PINB & 4; }

#elif defined (__AVR_ATmega32__)
#define __CPU	AVR_ATmega32

static inline void AVR_ATmega32_setup(void)
{
	// Clock is set via fuse, at least to 8MHz
	// Clock is set via fuse to 8MHz
	TCCR0 = 0x03;			// Prescaler 1/64
	MCUCR |= (1 << ISC00);	// Interrupt on both level changes
}

static inline void AVR_ATmega32_mask_owpin(void) { GIMSK &= ~(1 << INT0); }
static inline void AVR_ATmega32_unmask_owpin(void) { GIFR |= (1 << INTF0); GIMSK |= (1 << INT0); }
static inline void AVR_ATmega32_set_owtimer(uint8_t timeout)
{
	TCNT0 = ~timeout;	// overrun at 0xFF
	TIFR |= (1 << TOV0);
	TIMSK |= (1 << TOIE0);
}
static inline void AVR_ATmega32_clear_owtimer(void) { TCNT0 = 0; TIMSK &= ~(1 << TOIE0); }

// use INT0 pin (PORT D2)
static inline void AVR_ATmega32_owpin_setup(void) { PORTD &= ~4; DDRD &= ~4; }
static inline void AVR_ATmega32_owpin_low(void) { DDRD |= 4; }
static inline void AVR_ATmega32_owpin_hiz(void) { DDRD &= ~4; }
static inline uint8_t AVR_ATmega32_owpin_value(void) { return PIND & 4; }

#define BAUDRATE 38400
#elif defined (__AVR_ATtiny84__)
#define __CPU	AVR_ATtiny84

static inline void AVR_ATtiny84_setup(void)
{
	CLKPR = 0x80;	// Prepare to ...
	CLKPR = 0x00;	// ... set to 8.0 MHz
	MCUCR |= (1 << ISC00);	// Interrupt on both level changes
}

static inline void AVR_ATtiny84_mask_owpin(void) { GIMSK &= ~(1 << INT0); }
static inline void AVR_ATtiny84_unmask_owpin(void) { GIFR |= (1 << INTF0); GIMSK |= (1 << INT0); }
static inline void AVR_ATtiny84_set_owtimer(uint8_t timeout)
{
	TCNT0 = ~timeout;	// overrun at 0xFF
	TIFR0 |= (1 << TOV0);
	TIMSK0 |= (1 << TOIE0);
}
static inline void AVR_ATtiny84_clear_owtimer(void) { TCNT0 = 0; TIMSK0 &= ~(1 << TOIE0); }

// use INT0 pin (PORT B2)
static inline void AVR_ATtiny84_owpin_setup(void) { PORTB &= ~4; DDRB &= ~4; }
static inline void AVR_ATtiny84_owpin_low(void) { DDRB |= 4; }
static inline void AVR_ATtiny84_owpin_hiz(void) { DDRB &= ~4; }
static inline uint8_t AVR_ATtiny84_owpin_value(void) { return PINB & 4; }

#elif defined (__AVR_ATmega168__)
#define __CPU	AVR_ATmega168

static inline void AVR_ATmega168_setup(void)
{
	// Clock is set via fuse, at least to 8MHz
	TCCR0A = 0;
	TCCR0B = 0x03;		// Prescaler 1/64
	EICRA = (1<<ISC00); // interrupt of INT0 (pin D2) on both level changes
}

static inline void AVR_ATmega168_mask_owpin(void) { EIMSK &= ~(1 << INT0); }
static inline void AVR_ATmega168_unmask_owpin(void) { EIFR |= (1 << INTF0); EIMSK |= (1 << INT0); }
static inline void AVR_ATmega168_set_owtimer(uint8_t timeout)
{
	TCNT0 = ~timeout;	// overrun at 0xFF
	TIFR0 |= (1 << TOV0);
	TIMSK0 |= (1 << TOIE0);
}
static inline void AVR_ATmega168_clear_owtimer(void) { TCNT0 = 0; TIMSK0 &= ~(1 << TOIE0); }

// use INT0 pin (PORT D2)
static inline void AVR_ATmega168_owpin_setup(void) { PORTD &= ~4; DDRB &= ~4; }
static inline void AVR_ATmega168_owpin_low(void) { DDRD |= 4; }
static inline void AVR_ATmega168_owpin_hiz(void) { DDRD &= ~4; }
static inline uint8_t AVR_ATmega168_owpin_value(void) { return PIND & 4; }

#else
#error "CPU not supported (or at least not tested)"
#endif

// some macro magic for the functions
#define _COMPOSE(c, f)		c ## f
#define _SETUP(c)			_COMPOSE(c, _setup)
#define _MASK_OWPIN(c)		_COMPOSE(c, _mask_owpin)
#define _UNMASK_OWPIN(c)	_COMPOSE(c, _unmask_owpin)
#define _SET_OWTIMER(c)		_COMPOSE(c, _set_owtimer)
#define _CLEAR_OWTIMER(c)	_COMPOSE(c, _clear_owtimer)
#define _OWPIN_SETUP(c)		_COMPOSE(c, _owpin_setup)
#define _OWPIN_LOW(c)		_COMPOSE(c, _owpin_low)
#define _OWPIN_HIZ(c)		_COMPOSE(c, _owpin_hiz)
#define _OWPIN_VALUE(c)		_COMPOSE(c, _owpin_value)


/* cpu independent functions, but with names that show the CPU (during debug),
 *   unmask usually acknowledges the interrupt as-well
 */
#define cpu_setup		_SETUP(__CPU)
#define mask_owpin		_MASK_OWPIN(__CPU)
#define unmask_owpin	_UNMASK_OWPIN(__CPU)
#define set_owtimer		_SET_OWTIMER(__CPU)
#define clear_owtimer	_CLEAR_OWTIMER(__CPU)
#define owpin_setup		_OWPIN_SETUP(__CPU)
#define owpin_low		_OWPIN_LOW(__CPU)
#define owpin_hiz		_OWPIN_HIZ(__CPU)
#define owpin_value		_OWPIN_VALUE(__CPU)


#endif /* FEATURES_H */
