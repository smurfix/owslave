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

/* Based on work published at http://www.mikrocontroller.net/topic/44100 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
 
#include "dev_data.h"
#ifndef DEBUG_ONEWIRE
#define NO_DEBUG
#endif
#include "debug.h"

#include "uart.h"
#include "features.h"
#include "onewire.h"
#include "moat.h"

typedef union {
	CFG_DATA(owid) ow_addr;
	uint8_t addr[8];
} ow_addr_t;
extern ow_addr_t ow_addr;

extern volatile uint8_t bitp;  // mask of current bit
extern volatile uint8_t bytep; // position of current byte
extern volatile uint8_t cbuf;  // char buffer, current byte to be (dis)assembled

#ifndef EICRA
#define EICRA MCUCR
#endif

// use timer 2 (if present), because (a) we reset the prescaler and (b) the
// scaler from T2 is more accurate: 4 µsec vs. 8 µsec, in 8-MHz mode
#ifdef ONEWIRE_USE_T2
#define PRESCALE 32
#else
#ifdef ONEWIRE_USE_T0
#define PRESCALE 64
#else
#error "Which timer should we use?"
#endif
#endif

// Frequency-dependent timing macros
#if 0 // def DBGPIN // additional overhead for playing with the trace pin
#define _ADD_T 1
#else
#define _ADD_T 0
#endif
// T_(x)-y => value for setting the timer
// x: nominal time in microseconds
// y: overhead: increase by 1 for each 64 clock ticks
#define T_(c) ((F_CPU/PRESCALE)/(1000000/c)-_ADD_T)
#define OWT_MIN_RESET T_(410)
#define OWT_RESET_PRESENCE (T_(40)-1)
#define OWT_PRESENCE (T_(160)-1)
#define OWT_READLINE (T_(30)-2)
#define OWT_LOWTIME (T_(40)-2)

#if (OWT_MIN_RESET>240)
#error Reset timing is broken, your clock is too fast
#endif
#if (OWT_READLINE<1)
#error Read timing is broken, your clock is too slow
#endif

#if ONEWIRE_IRQNUM == -1
#define EN_OWINT() do {IMSK|=(1<<INT0);IFR|=(1<<INTF0);}while(0)  //enable interrupt 
#define DIS_OWINT() do {IMSK&=~(1<<INT0);} while(0)  //disable interrupt
#define SET_RISING() do {EICRA|=(1<<ISC01)|(1<<ISC00);}while(0)  //set interrupt at rising edge
#define SET_FALLING() do {EICRA|=(1<<ISC01);EICRA&=~(1<<ISC00);} while(0) //set interrupt at falling edge
#define CHK_INT_EN() (IMSK&(1<<INT0)) //test if pin interrupt enabled
#define PIN_INT INT0_vect  // the interrupt service routine
#elif ONEWIRE_IRQNUM == -2
#define EN_OWINT() do {IMSK|=(1<<INT1);IFR|=(1<<INTF1);}while(0)  //enable interrupt 
#define DIS_OWINT() do {IMSK&=~(1<<INT1);} while(0)  //disable interrupt
#define SET_RISING() do {EICRA|=(1<<ISC11)|(1<<ISC10);}while(0)  //set interrupt at rising edge
#define SET_FALLING() do {EICRA|=(1<<ISC11);EICRA&=~(1<<ISC10);} while(0) //set interrupt at falling edge
#define CHK_INT_EN() (IMSK&(1<<INT1)) //test if pin interrupt enabled
#define PIN_INT INT1_vect  // the interrupt service routine
#else
#error generic pin change interrupts are not yet supported
#endif
//Timer Interrupt
//Timer Interrupt

#ifdef ONEWIRE_USE_T2
#define EN_TIMER() do {TIMSK2 |= (1<<TOIE2); TIFR2|=(1<<TOV2);}while(0) //enable timer interrupt
#define DIS_TIMER() do {TIMSK2 &= ~(1<<TOIE2);} while(0) // disable timer interrupt
#define SET_TIMER(x) do { GTCCR = (1<<PSRASY); TCNT2=(uint8_t)~(x); } while(0) // reset prescaler
#define TIMER_INT ISR(TIMER2_OVF_vect) //the timer interrupt service routine

#else
#ifdef __AVR_ATmega8__
// Not sure if this is valid for others as well?
#define GTCCR SFIOR
#define PSRSYNC PSR10
#else
#ifndef PSRSYNC
#define PSRSYNC PSR0
#endif
#endif
#define EN_TIMER() do {TIMSK0 |= (1<<TOIE0); TIFR0|=(1<<TOV0);}while(0) //enable timer interrupt
#define DIS_TIMER() do {TIMSK0 &= ~(1<<TOIE0);} while(0) // disable timer interrupt
#define SET_TIMER(x) do { GTCCR = (1<<PSRSYNC); TCNT0=(uint8_t)~(x); } while(0) // reset prescaler
#define TIMER_INT ISR(TIMER0_OVF_vect) //the timer interrupt service routine
#endif

// stupidity
#ifndef TIMER0_OVF_vect
#  define TIMER0_OVF_vect TIM0_OVF_vect
#endif

#define SET_LOW() do { ONEWIRE_DDR|=ONEWIRE_PBIT;} while(0)  //set 1-Wire line to low
#define CLEAR_LOW() do {ONEWIRE_DDR&=~ONEWIRE_PBIT;} while(0) //set 1-Wire pin as input

// Initialise the hardware
void onewire_init(void);

//States / Modes
typedef enum {
	OWM_SLEEP,  //Waiting for next reset pulse
	OWM_IN_RESET,  //waiting of rising edge from reset pulse
	OWM_AFTER_RESET,  //Reset pulse received 
	OWM_PRESENCE,  //sending presence pulse
	OWM_SEARCH_ZERO,  //SEARCH_ROM algorithm
	OWM_SEARCH_ONE,
	OWM_SEARCH_READ,

	OWM_IDLE, // non-IRQ mode starts here (mostly)
	OWM_READ, // reading some bits
	OWM_WRITE, // writing some bits
} mode_t;
volatile mode_t mode; //state

//next high-level state
typedef enum {
	OWX_IDLE, // nothing is happening
	OWX_SELECT, // will read a selector
	OWX_COMMAND, // will read a command
	OWX_RUNNING, // in user code
} xmode_t;
extern volatile xmode_t xmode;

// Write this bit at next falling edge from master.
// We use a whole byte for this for assembly speed reasons.
typedef enum {
	OWW_WRITE_0, // used in assembly
	OWW_WRITE_1,
	OWW_NO_WRITE,
} wmode_t;
extern volatile wmode_t wmode;
extern volatile uint8_t actbit; // current bit. Keeping this saves 14bytes ROM

static inline void start_reading(uint8_t bits) {
	mode = OWM_READ;
	cbuf=0;
	bitp=1<<(8-bits);
}
// same thing for within the timer interrupt
#define START_READING(bits) do { \
	lmode = OWM_READ; \
	cbuf=0; \
	lbitp=1<<(8-bits); \
} while(0)

#define wait_complete(c) _wait_complete()
//static inline void wait_complete(char c)
void _wait_complete(void);

void next_command(void) __attribute__((noreturn));

#ifdef NEED_BITS
void xmit_bit(uint8_t val);
#endif

// It is a net space win not to inline this.
void xmit_byte(uint8_t val);

uint16_t xmit_byte_crc(uint16_t crc, uint8_t val);
uint16_t xmit_bytes_crc(uint16_t crc, uint8_t *buf, uint8_t len);

#if 0
uint8_t rx_ready(void);
#endif

uint8_t recv_any_in(void);
#ifdef NEED_BITS
void recv_bit(void);
#endif

void recv_byte(void);

uint16_t recv_bytes_crc(uint16_t crc, uint8_t *buf, uint8_t len);

uint16_t crc16(uint16_t r, uint8_t x);

void onewire_poll(void);

// NOTE this disables interrupts!
void set_idle(void);

