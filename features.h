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

/* these are the current project settings, should be configurable in a way */

/* HAVE_UART is really critical, the POLLED_TRANSMITTER works better
 *   as it wont delay the 1-wire interrupts. This is especially true if large
 *   amounts of data are output (pin debug and edge debug on!)
 */
#define HAVE_UART
#define POLLED_TRANSMITTER
#define BAUDRATE 57600
#define F_CPU 16000000

/* some basic typedef, that should be very portable */
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned long u_long;


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
#include "avr.h"
#else
#define __CPU	CortexM3
/*!
 * currently these are NO-OPs, compilability test only!
 */
static inline void CortexM3_setup(void)
{
}

static inline void CortexM3_mask_owpin(void) { }
static inline void CortexM3_unmask_owpin(void) { }
static inline void CortexM3_set_owtimer(u_char timeout)
{
}
static inline void CortexM3_clear_owtimer(void) { }

//
static inline void CortexM3_owpin_setup(void) {  }
static inline void CortexM3_owpin_low(void) { }
static inline void CortexM3_owpin_hiz(void) { }
static inline u_char CortexM3_owpin_value(void) { return 1; }

// not really
#define OW_TIMER_ISR() void __attribute__ ((interrupt)) __cs3_isr_systick (void)
#define OW_PINCHANGE_ISR() void __attribute__ ((interrupt)) __cs3_isr_external_0 (void)
#define cli()
#define sli()

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
