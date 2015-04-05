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
#include <setjmp.h>

#include "features.h"
#include "onewire.h"
#include "uart.h"
#include "dev_data.h"
#include "debug.h"

#define PRESCALE 64

union {
	CFG_DATA(owid) ow_addr;
	uint8_t addr[8];
} ow_addr;

volatile uint8_t bitp;  // mask of current bit
volatile uint8_t bytep; // position of current byte
volatile uint8_t cbuf;  // char buffer, current byte to be (dis)assembled

static jmp_buf end_out;
static void go_out(void) __attribute__((noreturn));
static void go_out(void) {
	longjmp(end_out,1); // saves bytes
}



#ifndef TIMSK
#define TIMSK TIMSK0
#endif
#ifndef TIFR
#define TIFR TIFR0
#endif
#ifndef EICRA
#define EICRA MCUCR
#endif

// Frequency-dependent timing macros
#ifdef DBGPIN // additional overhead for playing with the trace pin
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
#define OWT_READLINE (T_(30)-1)
#define OWT_LOWTIME (T_(60)-3)

#if (OWT_MIN_RESET>240)
#error Reset timing is broken, your clock is too fast
#endif
#if (OWT_READLINE<1)
#error Read timing is broken, your clock is too slow
#endif

#define EN_OWINT() do {IMSK|=(1<<INT0);IFR|=(1<<INTF0);}while(0)  //enable interrupt 
#define DIS_OWINT() do {IMSK&=~(1<<INT0);} while(0)  //disable interrupt
#define SET_RISING() do {EICRA|=(1<<ISC01)|(1<<ISC00);}while(0)  //set interrupt at rising edge
#define SET_FALLING() do {EICRA|=(1<<ISC01);EICRA&=~(1<<ISC00);} while(0) //set interrupt at falling edge
#define CHK_INT_EN() (IMSK&(1<<INT0)) //test if pin interrupt enabled
#define PIN_INT INT0_vect  // the interrupt service routine
//Timer Interrupt
#define EN_TIMER() do {TIMSK |= (1<<TOIE0); TIFR|=(1<<TOV0);}while(0) //enable timer interrupt
#define DIS_TIMER() do {TIMSK &= ~(1<<TOIE0);} while(0) // disable timer interrupt

// always use timer 0
#define TCNT_REG TCNT0  //register of timer-counter
#define TIMER_INT ISR(TIMER0_OVF_vect) //the timer interrupt service routine

// stupidity
#ifndef TIMER0_OVF_vect
#  define TIMER0_OVF_vect TIM0_OVF_vect
#endif

#define SET_LOW() do { OWDDR|=(1<<ONEWIREPIN);} while(0)  //set 1-Wire line to low
#define CLEAR_LOW() do {OWDDR&=~(1<<ONEWIREPIN);} while(0) //set 1-Wire pin as input

static void do_select(uint8_t cmd);

// Initialise the hardware
void
onewire_init(void)
{
#ifdef __AVR_ATtiny13__
	TCCR0A = 0;
	TCCR0B = 0x03;	// Prescaler 1/64

	MCUCR |= (1 << ISC00);		  // Interrupt on both level changes

#elif defined(__AVR_ATtiny25__)
	MCUCR |= (1 << ISC00);		  // Interrupt on both level changes

#elif defined(__AVR_ATtiny84__)
	MCUCR |= (1 << ISC00);		  // Interrupt on both level changes

#elif defined (__AVR_ATmega8__)
	// Clock is set via fuse
	// CKSEL = 0100;   Fuse Low Byte Bits 3:0

	TCCR0 = 0x03;	// Prescaler 1/64

	MCUCR |= (1 << ISC00);		  // Interrupt on both level changes

#elif defined (__AVR_ATmega168__) || defined (__AVR_ATmega88__)
	// Clock is set via fuse

	TCCR0A = 0;
	TCCR0B = 0x03;	// Prescaler 1/64

	EICRA = (1<<ISC00); // interrupt of INT0 (pin D2) on both level changes

#else
#error "Timer/IRQ setup for your CPU undefined"
#endif

	OWPORT &= ~(1 << ONEWIREPIN);
	OWDDR &= ~(1 << ONEWIREPIN);

	cfg_read(owid, ow_addr.ow_addr);

	// init application-specific code
	init_state();

	IFR |= (1 << INTF0);
	IMSK |= (1 << INT0);
}

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
volatile xmode_t xmode;

// Write this bit at next falling edge from master.
// We use a whole byte for this for assembly speed reasons.
typedef enum {
	OWW_WRITE_0, // used in assembly
	OWW_WRITE_1,
	OWW_NO_WRITE,
} wmode_t;
volatile wmode_t wmode;
volatile uint8_t actbit; // current bit. Keeping this saves 14bytes ROM

static inline void clear_timer(void)
{
	//DBG_C('t');
	//TCNT0 = 0;
	TIMSK &= ~(1 << TOIE0);	   // turn off the timer IRQ
}

void next_idle(void) __attribute__((noreturn));
void next_idle(void)
{
	if(mode > OWM_PRESENCE)
		set_idle();
	//DBGS_P(".e1");
	go_out();
}

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
static inline void _wait_complete(void)
{
//	if(bitp || (wmode != OWW_NO_WRITE))
//		DBG_C(c);
	while(1) {
		if (mode < OWM_IDLE) {
//			DBGS_P("s5");
			next_idle();
		}
		if(!bitp && (wmode == OWW_NO_WRITE)) {
			DBG_OFF();
			return;
		}
#ifdef HAVE_UART
		uart_poll();
#endif
		update_idle(1); // actbit
	}
}

void next_command(void) __attribute__((noreturn));
void next_command(void)
{
	wait_complete('n');
	start_reading(8);
	//DBGS_P(".e4");

	xmode = OWX_COMMAND;
	go_out();
}

static inline void
xmit_any(uint8_t val, uint8_t len)
{
	wait_complete('w');
	cli();
	if(mode == OWM_READ || mode == OWM_IDLE)
		mode = OWM_WRITE;
	if (mode != OWM_WRITE || xmode < OWX_RUNNING) {
		// DBGS_P("\nErr xmit ");
		next_idle();
	}

	bitp = 1 << (8-len);
	cbuf = val;
	if(CHK_INT_EN()) {
		// next is pin interrupt
		wmode = (cbuf & bitp) ? OWW_WRITE_1 : OWW_WRITE_0;
		bitp <<= 1;
	}
	// otherwise the timer interrupt will do this
	// can't simply switch off the timer here because we might still be
	// writing a zero

	sei();
	DBG_OFF();
}

#ifdef NEED_BITS
void xmit_bit(uint8_t val)
{
	xmit_any(!!val,1);
}
#endif

// It is a net space win not to inline this.
void xmit_byte(uint8_t val)
{
	xmit_any(val,8);
}

#if 0
uint8_t rx_ready(void)
{
	if (mode <= OWM_IDLE)
		return 1;
	return !bitp;
}
#endif

static inline void
recv_any(uint8_t len)
{
	wait_complete('j');
	cli();
	if(mode == OWM_WRITE || mode == OWM_IDLE)
		mode = OWM_READ;
	if (mode != OWM_READ || xmode < OWX_RUNNING) {
		DBG_P("\nState error recv! ");
		DBG_X(mode);
		DBG_C('\n');
		next_idle();
	}
	bitp = 1 << (8-len);
	cbuf = 0;
	sei();
	DBG_OFF();
}


uint8_t
recv_any_in(void)
{
	wait_complete('i');
	if (mode != OWM_READ) {
		DBGS_P(".e2");
		go_out();
	}
	mode = OWM_IDLE;
	return cbuf;
}
#ifdef NEED_BIT
void
recv_bit(void)
{
	recv_any(1);
	DBG_C('_');
}
#endif

void
recv_byte(void)
{
	recv_any(8);
}

// this code is from owfs
static uint8_t parity_table[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

uint16_t
crc16(uint16_t r, uint8_t x)
{
	uint16_t c = (x ^ (r & 0xFF));
	r >>= 8;
	if (parity_table[c & 0x0F] ^ parity_table[(c >> 4) & 0x0F])
		r ^= 0xC001;
	r ^= (c <<= 6);
	r ^= (c << 1);
        return r;
}

void onewire_poll(void) {
#ifdef HAVE_UART
	volatile unsigned long long int x=0;
#endif

	setjmp(end_out);
	while (1) {
#ifdef HAVE_UART
		if(++x == 100000ULL) {
			x=0;
			DBGS_C('/');
		}
#endif
		uart_poll();
		DBG_OFF();
		if(!bitp) {
			xmode_t lxmode = xmode;
			if(lxmode == OWX_SELECT) {
				//DBG_C('S');
				//DBG_X(cbuf);
				xmode = OWX_RUNNING;
				do_select(cbuf);
			}
			else if(lxmode == OWX_COMMAND) {
				//DBG_C('C');
				//DBG_X(cbuf);
				xmode = OWX_RUNNING;
				do_command(cbuf);
			}
			else
				update_idle(2);
		} else
			update_idle(1);

		// RESET processing takes longer.
		// update_idle((mode == OWM_SLEEP) ? 100 : (mode <= OWM_AFTER_RESET) ? 20 : (mode < OWM_IDLE) ? 8 : 1); // TODO

#ifdef HAVE_TIMESTAMP
		unsigned char n = sizeof(tsbuf)/sizeof(tsbuf[0]);
		while(tbpos < n && n > 0) {
			uint16_t this_tb = tsbuf[--n];
			DBG_Y(this_tb-last_tb);
			last_tb=this_tb;

			DBG_C(lev ? '^' : '_');
			lev = 1-lev;
			cli();
			if(tbpos == n) tbpos = sizeof(tsbuf)/sizeof(tsbuf[0]);
			sei();
		}
		if (n == 0) {
			DBG_P("<?>");
			tbpos = sizeof(tsbuf)/sizeof(tsbuf[0]);
		}
#endif
		if (mode == OWM_SLEEP)
			return;
	}
}

void set_idle(void)
{
	/* This code will fail to recognize a reset if we're already in one.
	   Should happen rarely enough not to matter. */
	cli();
#ifdef HAVE_UART // mode is volatile
	if(mode != OWM_SLEEP) {
		DBGS_P(">idle:");
		DBGS_X(mode);
		DBGS_C(' ');
		DBGS_X(xmode);
		DBGS_C(' ');
		DBGS_X(bitp);
		DBGS_NL();
	}
#endif
	DBG_OFF();
	//DBG_OUT();

	mode = OWM_SLEEP;
	xmode = OWX_IDLE;
	wmode = OWW_NO_WRITE;
	CLEAR_LOW();
	DIS_TIMER();
	SET_FALLING();
	EN_OWINT();
	DBG_C('s');
	sei();
}

static inline void do_select(uint8_t cmd)
{
	uint8_t i;

	switch(cmd) {
	case 0xF0: // SEARCH_ROM; handled in interrupt
		mode = OWM_SEARCH_ZERO;
		bytep = 0;
		bitp = 1;
		cbuf = ow_addr.addr[0];
		actbit = cbuf&1;
		wmode = actbit ? OWW_WRITE_1 : OWW_WRITE_0;
		return;
#ifdef CONDITIONAL_SEARCH
	case 0xEC: // CONDITIONAL SEARCH
		if (!condition_met())
			next_idle();
		/* FALL THRU */
#endif
	case 0x55: // MATCH_ROM
		recv_byte();
		for (i=0;;i++) {
			uint8_t b = recv_byte_in();
			if (b != ow_addr.addr[i])
				next_idle();
			if (i < 7)
				recv_byte();
			else
				break;
		}
		next_command();
#ifdef SINGLE_DEVICE
	case 0xCC: // SKIP_ROM
		next_command();
	case 0x33: // READ_ROM
		for (i=0;i<8;i++)
			xmit_byte(ow_addr.addr[i]);
		next_idle();
#endif
	default:
		DBGS_P("\n?CS ");
		DBGS_X(cmd);
		DBGS_C('\n');
		next_idle();
	}
}

TIMER_INT {
	//Read input line state first
	uint8_t p = !!(OWPIN&(1<<ONEWIREPIN));
	mode_t lmode=mode;
//	if(lmode == OWM_READ || lmode == OWM_SEARCH_READ)
//		DBG_T(p+1);
//	if (lmode > OWM_PRESENCE && lmode != OWM_WRITE)
	if(DBG_PIN())
		DBG_OFF();
	else
		DBG_ON();
	wmode_t lwmode=OWW_NO_WRITE; // wmode; //let these variables be in registers
	uint8_t lbitp=bitp;
	uint8_t lactbit=actbit;

	if (CHK_INT_EN()) {
		// reset pulse?
		if (p==0) { 
#if 0
			if(lmode != OWM_SLEEP) {
				DBGS_P("\nReset ");
				DBGS_X(lmode);
				DBGS_NL();
			}
#endif
			lmode=OWM_IN_RESET;  //wait for rising edge
			SET_RISING(); 
			//DBG_C('R');
		}
		DIS_TIMER();
	} else
	switch (lmode) {
	case OWM_IDLE:
		DBGS_P("\nChk Idle!\n");
		lmode = OWM_SLEEP;  // time overrun, nothing can be done
		break;
	case OWM_SLEEP: // should have been caught by CHK_INT_EN() above
		DBGS_P("\nChk Sleep!\n");
		break;
	case OWM_IN_RESET: // should not happen here
		DBGS_P("\nChk Reset!\n");
		break;
	case OWM_AFTER_RESET:  //Time after reset is finished, now go to presence state
		lmode=OWM_PRESENCE;
		SET_LOW();
		TCNT_REG=~OWT_PRESENCE;
		DIS_OWINT();  // wait for presence is done
		break;
	case OWM_PRESENCE:
		CLEAR_LOW();  //Presence is done, now wait for a command
		DIS_TIMER();
		START_READING(8);
		xmode = OWX_SELECT;
		break;
	case OWM_READ:
		if(lbitp) {
			//DBG_C(p ? 'B' : 'b');
			if (p)  // Set bit if line high 
				cbuf |= lbitp;
			lbitp <<= 1;
		} else {
			// Overrun!
			DBGS_P("\nRead OVR!\n");
			lmode = OWM_SLEEP;
		}
		break;
	case OWM_WRITE:
		CLEAR_LOW();
		if (lbitp) {
			lwmode = (cbuf & lbitp) ? OWW_WRITE_1 : OWW_WRITE_0;
			lbitp <<= 1;
		} else {
			// Overrun!
			DBGS_P("\nWrite OVR!\n");
			lmode = OWM_SLEEP;
		}
		break;
	case OWM_SEARCH_ZERO:
		CLEAR_LOW();
		lmode = OWM_SEARCH_ONE;
		lwmode = lactbit ? OWW_WRITE_0 : OWW_WRITE_1;
		break;
	case OWM_SEARCH_ONE:
		CLEAR_LOW();
		lmode = OWM_SEARCH_READ;
		break;
	case OWM_SEARCH_READ:
		//DBG_C(p ? 'B' : 'b');
		if (p != lactbit) {  //check master bit
			//DBG_C('d');
			lmode = OWM_SLEEP;  //not the same: go to sleep
			break;
		}

		lbitp=(lbitp<<1);  //prepare next bit
		if (!lbitp) {
			uint8_t lbytep = bytep;
			lbytep++;
			bytep=lbytep;
			if (lbytep>=8) {
				START_READING(8);
				//DBG_P("S2");
				xmode = OWX_COMMAND;
				break;
			}
			lbitp=1;
			cbuf = ow_addr.addr[lbytep];
		}
		lmode = OWM_SEARCH_ZERO;
		lactbit = !!(cbuf&lbitp);
		lwmode = lactbit ? OWW_WRITE_1 : OWW_WRITE_0;
		break;
	}
	if (lmode == OWM_SLEEP)
		DIS_TIMER();
	if (lmode != OWM_PRESENCE) { 
		TCNT_REG=~(OWT_MIN_RESET-OWT_READLINE);  //OWT_READLINE around OWT_LOWTIME
		EN_OWINT();
	}
	mode=lmode;
	wmode=lwmode;
	bitp=lbitp;
	actbit=lactbit;
	DBG_OFF();
	DBG_ON();
}

// 1wire level change.
// Do this in assembler so that writing a zero is _fast_.
void real_PIN_INT(void) __attribute__((signal));
void INT0_vect(void) __attribute__((naked));
void INT0_vect(void) {
	// WARNING: No command in here may change the status register!
	asm("     push r24");
	asm("     push r25");
	asm("     lds r24,wmode");
	asm("     ldi r25,0");
	asm("     cpse r24,r25");
	asm("     rjmp .Lj");
	SET_LOW();
	asm(".Lj: pop r25");
	asm("     pop r24");
	asm("     rjmp real_PIN_INT");
	asm("     nop");
}

// This generates a spurious warning
// which unfortunately cannot be turned off with GCC <4.8.2
void real_PIN_INT(void) {
	DIS_OWINT(); //disable interrupt, only in OWM_SLEEP mode it is active
#ifdef DBGPIN // modes are volatile
	if (mode > OWM_PRESENCE) {
		DBG_ON();
		if (wmode == OWW_NO_WRITE)
			DBG_OFF();
	}
#endif
#if 0
	if (mode == OWM_SLEEP) { // don't report anything
	} else if (wmode != OWW_NO_WRITE) {
		DBG_C(wmode ? 'P' : 'p');
	} else {
		uint8_t p = OWPIN&(1<<ONEWIREPIN);
		DBG_C(p ? 'Q' : 'q');
	}
#endif
	//if (mode != OWM_SLEEP) DBG_C('a'+mode);
	wmode = OWW_NO_WRITE;

	switch (mode) {
	case OWM_PRESENCE:
		DBGS_P("\nChk Presence!\n");
		break;
	case OWM_IDLE:
		DBGS_P("\nChk Idle!\n");
		break;
	case OWM_SLEEP:
		TCNT_REG=~(OWT_MIN_RESET);
		EN_OWINT(); //any other edges will simply reset the timer
		break;
	//start of reading with falling edge from master, reading closed in timer isr
	case OWM_READ:
	case OWM_SEARCH_READ:   //Search algorithm waiting for receive or send
		TCNT_REG=~(OWT_READLINE); //wait a time for reading
		break;
	case OWM_SEARCH_ZERO:   //Search algorithm waiting for receive or send
	case OWM_SEARCH_ONE:   //Search algorithm waiting for receive or send
	case OWM_WRITE: //a bit is sending 
		TCNT_REG=~(OWT_LOWTIME);
		break;
	case OWM_IN_RESET:  //rising edge of reset pulse
		TCNT_REG=~(OWT_RESET_PRESENCE);  //wait before sending presence pulse
		mode=OWM_AFTER_RESET;
		SET_FALLING();
		//DBG_C('r');
		break;
	case OWM_AFTER_RESET: // some other chip was faster, assert my own presence signal now
		TCNT_REG=~0;
		break;
	}
	EN_TIMER();
//	if (mode > OWM_PRESENCE)
//		DBG_T(1);
}

