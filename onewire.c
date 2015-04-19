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

#include "features.h"
#include "onewire.h"
#include "uart.h"
#include "dev_data.h"
#include "debug.h"
#include "moat.h"

#define PRESCALE 64

union {
	CFG_DATA(owid) ow_addr;
	uint8_t addr[8];
} ow_addr;

volatile uint8_t bitp;   // mask of current bit
volatile uint8_t bytep;  // position of current byte
volatile uint8_t cbuf;   // char buffer, current byte to be (dis)assembled
volatile uint8_t cmdbuf; // current command to run
volatile uint8_t pin_poll;  // flag set by the wire interrupt
volatile uint8_t timer_poll;  // flag set by the timer interrupt
volatile uint8_t bitbuf; // bit input read by the timer IRQ


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
#define OWT_READLINE (T_(30)-1)
#define OWT_LOWTIME (T_(60)-2)

#if (OWT_MIN_RESET>240)
#error Reset timing is broken, your clock is too fast
#endif
#if (OWT_READLINE<1)
#error Read timing is broken, your clock is too slow
#endif

#define EN_OWINT() do {IFR|=(1<<INTF0);IMSK|=(1<<INT0);} while(0)  //enable interrupt 
#define DIS_OWINT() do {IMSK&=~(1<<INT0);} while(0)  //disable interrupt
#define SET_RISING() do {EICRA|=(1<<ISC01)|(1<<ISC00);}while(0)  //set interrupt at rising edge
#define SET_FALLING() do {EICRA|=(1<<ISC01);EICRA&=~(1<<ISC00);} while(0) //set interrupt at falling edge
#define CHK_INT_EN() (IMSK&(1<<INT0)) //test if pin interrupt enabled
#define PIN_INT INT0_vect  // the interrupt service routine
//Timer Interrupt
#define EN_TIMER() do {DBG_ON();TIFR|=(1<<TOV0); TIMSK|=(1<<TOIE0);}while(0) //enable timer interrupt
#define DIS_TIMER() do {TIMSK &= ~(1<<TOIE0);DBG_OFF();} while(0) // disable timer interrupt
//#define HAS_TIMER() (TIFR & (1<<TOV0)) // check for pending timer interrupt
#define HAS_TIMER() (TIMSK & (1<<TOIE0)) // check for pending timer interrupt
#define SET_TIMER(x) do { GTCCR = (1<<PSRSYNC); TCNT_REG=~(x); } while(0) // reset prescaler

// always use timer 0
#define TCNT_REG TCNT0  //register of timer-counter
#define TIMER_INT ISR(TIMER0_OVF_vect) //the timer interrupt service routine

// stupidity
#ifndef TIMER0_OVF_vect
#  define TIMER0_OVF_vect TIM0_OVF_vect
#endif

#define SET_LOW() do { OWDDR|=(1<<ONEWIREPIN);} while(0)  //set 1-Wire line to low
#define CLEAR_LOW() do {OWDDR&=~(1<<ONEWIREPIN);} while(0) //set 1-Wire pin as input

static void do_select(void);

static void real_PIN_INT(void);
static void real_TIMER_INT(void);

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

	IFR |= (1 << INTF0);
	IMSK |= (1 << INT0);

	set_idle();
}

//States / Modes
typedef enum {
	OWM_SLEEP,  //Waiting for next reset pulse
	OWM_IN_RESET,  //waiting of rising edge from reset pulse
	OWM_AFTER_RESET,  //Reset pulse received, before sending presence
	OWM_PRESENCE,  //during send of presence pulse
	OWM_SEARCH_ZERO,  //SEARCH_ROM algorithm
	OWM_SEARCH_ONE,
	OWM_SEARCH_READ,

	OWM_IDLE, // non-IRQ mode starts here (mostly)
	OWM_WRITE, // writing some bits
	OWM_READ, // reading some bits
	OWM_READ_SELECT, // reading some bits
	OWM_READ_COMMAND, // reading some bits
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
	OWW_READ,
} wmode_t;
volatile wmode_t wmode;
volatile uint8_t actbit; // current bit. Keeping this saves 14bytes ROM

void next_idle(void) __attribute__((noreturn));
void next_idle(void)
{
	if(mode > OWM_PRESENCE) {
		set_idle();
		sei();
	}
	//DBGS_P(".e1");
	go_out();
}

static inline void start_reading(mode_t _mode,uint8_t bits) {
	mode = _mode;
	wmode = OWW_READ;
	cbuf=0;
	bitp=1<<(8-bits);
}
// same thing for within the timer interrupt
#define START_READING(_mode,bits) do { \
	lmode = _mode; \
	lwmode = OWW_READ; \
	cbuf=0; \
	lbitp=1<<(8-bits); \
} while(0)

#define wait_complete(c) _wait_complete()
//static inline void wait_complete(char c)
static inline void _wait_complete(void)
{
//	if(bitp || (wmode != OWW_NO_WRITE))
//		DBG_C(c);
#if 0 // def HAVE_UART
	volatile unsigned long long int x=0;
#endif
	while(1) {
		if (mode < OWM_IDLE) {
			//DBGS_P("s5.");
			//DBGS_Y(mode);
			next_idle();
		}
		if(!bitp && (mode != OWM_WRITE || wmode == OWW_NO_WRITE)) {
			return;
		}
#if 0 // def HAVE_UART
		uart_poll();
#endif
		update_idle(1); // actbit
#if 0 // def HAVE_UART
		if(++x>=100000ULL) {
			x=0;
			DBGS_C('|');
		}
#endif
	}
}

void next_command(void) __attribute__((noreturn));
void next_command(void)
{
	wait_complete('n');
	start_reading(OWM_READ_COMMAND,8);
	//DBGS_P(".e4");
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
		DBGS_P("\nErr xmit ");
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
}

#define XMIT_BYTE(val) do { \
	lmode = OWM_WRITE; \
	cbuf = val; \
	lwmode = (cbuf & 1) ? OWW_WRITE_1 : OWW_WRITE_0; \
	lbitp = 2; \
} while(0)

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
#if 0
	if (mode != OWM_READ || xmode < OWX_RUNNING) {
		DBG_P("\nState error recv! ");
		DBG_X(mode);
		DBG_C('\n');
		next_idle();
	}
#endif
	bitp = 1 << (8-len);
	cbuf = 0;
	sei();
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

char _onewire_poll(void) __attribute__((OS_task));
char _onewire_poll(void) {
	DBG(0x3A);
	real_PIN_INT();
	real_TIMER_INT();

#if 0 // def HAVE_UART
	if(++x>=100000ULL) {
		x=0;
		DBGS_C('\\');
		DBGS_Y(mode);
	}
#endif
	{
#if 0
		cli();
		if(!HAS_TIMER()) {
			DBG(0x38);
			DBG_OFF();
		} else if(TIFR&(1<<TOV0)) {
			DBG(0x32);
			DBG_OFF();
		} else {
			DBG(0x33);
			DBG_ON();
			DBG(TCNT_REG);
		}
		sei();
#endif
		uart_poll();

		if(!bitp) {
			DBG(0x37);
			xmode_t lxmode = xmode;
			if(lxmode == OWX_SELECT) {
				DBG(0x32);
				//DBG_C('S');
				//DBG_X(cbuf);
				xmode = OWX_RUNNING;
				do_select();
			} else if(lxmode == OWX_COMMAND) {
				DBG(0x23);
				//DBG_C('C');
				//DBG_X(cbuf);
				xmode = OWX_RUNNING;
				do_command(cmdbuf);
				wait_complete('d');
				set_idle();
				sei();
			} else {
				DBG(0x34);
				update_idle(2);
				DBG(0x35);
			}
		} else {
			DBG(0x36);
			update_idle(1);
		}

		// RESET processing takes longer.
		// update_idle((mode == OWM_SLEEP) ? 100 : (mode <= OWM_AFTER_RESET) ? 20 : (mode < OWM_IDLE) ? 8 : 1); // TODO

		if (mode == OWM_IDLE) {
			DBG(0x2b);
			set_idle();
			sei();
		}
		if (mode == OWM_SLEEP) {
			DBG(0x2a);
			return 1;
		}
	}
	DBG(0x25);
	return 0;
}

void onewire_poll(void) __attribute__((OS_task));
void onewire_poll(void) {
#if 0 // def HAVE_UART
	volatile unsigned long long int x=0;
#endif

	DBG(0x2B);
	while (1) if (_onewire_poll()) break;
	DBG(0x2C);
}

void set_idle(void)
{
	/* This code will fail to recognize a reset if we're already in one.
	   Should happen rarely enough not to matter. */
	DBG(0x22);
	cli();
#if 0 // def HAVE_UART // mode is volatile
	if(mode != OWM_SLEEP && mode != OWM_IDLE) {
		DBGS_P(">idle:");
		DBGS_X(mode);
		DBGS_C(' ');
		DBGS_X(xmode);
		DBGS_C(' ');
		DBGS_X(bitp);
		DBGS_NL();
	}
#endif
	CLEAR_LOW();
	DIS_TIMER();
	SET_FALLING();
	EN_OWINT();

	mode = OWM_SLEEP;
	xmode = OWX_IDLE;
	wmode = OWW_NO_WRITE;
	pin_poll = 0;
	timer_poll = 0;
}


static inline void do_select(void)
{
	uint8_t i;

	switch(cmdbuf) {
	case 0xF0: // SEARCH_ROM; handled in interrupt
		return;
#ifdef CONDITIONAL_SEARCH
	case 0xEC: // CONDITIONAL SEARCH
		if (!condition_met())
			next_idle();
		break;
#endif
	case 0x55: // MATCH_ROM
		DBG_C('m');
		for (i=0;;i++) {
			uint8_t b = recv_byte_in();
			if (b != ow_addr.addr[i]) {
				DBG_C('q');
				DBG_X(b);
				next_idle();
			}
			if (i < 7)
				recv_byte();
			else
				break;
		}
		DBG_C('n');
		next_command();
#ifdef SINGLE_DEVICE
	case 0xCC: // SKIP_ROM
		next_command();
	case 0x33: // READ_ROM
		for (i=1;i<8;i++)
			xmit_byte(ow_addr.addr[i]);
		next_idle();
#endif
	default:
		DBGS_P("\n?CS ");
		DBGS_X(cmdbuf);
		DBGS_C('\n');
		next_idle();
	}
}

//TIMER_INT {

void TIMER0_OVF_vect(void) __attribute__((naked));
void TIMER0_OVF_vect(void) {
	// WARNING: No command in here may change the status register!
	volatile register uint8_t t asm ("r24");
	volatile register uint8_t z asm ("r25");
	DBG_OFF();
	DBG_ON();
	DBG_OFF();
	asm("     push r24");
	asm("     push r25");
	DBGA(0x3D,t);
	t = wmode;
	// if wmode == OWW_READ
		z = OWW_READ;
		asm("     cpse %0,%1" :: "r"(z),"r"(t));
		asm("     rjmp .Ld");

	// then copy the pin into bit0 of bitbuf
		asm("     ldi %0,0" : "=r"(z));
		asm("     sbic %0, %1" :: "i"(((int)&OWPIN)-__SFR_OFFSET),"i"(ONEWIREPIN), "r"(z));
		asm("     ldi %0,1" : "=r"(z));
		asm("     sts %0,%1" : "=m" (bitbuf) : "r"(z));
		DBGA(0x3C,t);
		asm("     rjmp .Le");
	// else if wmode == OWW_WRITE_0
		asm(".Ld:");
		z = OWW_WRITE_0;
		asm("     cpse r24,r25" :: "r"(z),"r"(t));
		asm("     rjmp .Le");
		CLEAR_LOW();
		DBGA(0x39,t);

	// end if

	// Now set timer_poll. This code will set it to 1 if it was zero, else 3.
	// We use "sbrc" (skip if bit0 is clear) to save on comparisons,
	// which would require saving the condition register.
	asm(".Le:");
	t = timer_poll;
	asm("     ldi %0,1" : "=r"(z));
	asm("     sbrc %0,0" :: "r"(t), "r"(z));
	asm("     ldi %0,3" : "=r"(z));
	timer_poll = z;
	DBGA(0x38,t);

	asm("     pop r25");
	asm("     pop r24");
	if(HAS_TIMER()) DBG_ON();
	asm("     reti");
}

void real_TIMER_INT(void) {
	//Read input line state first
	//and copy a few globals to registers
	if(!timer_poll) return;
	DBG(0x01);
	if(pin_poll) {
		DBG(0x02);
		DBGS_P("\nOVt2 ");
		DBGS_Y(mode);
		DBGS_C('\n');
		set_idle();
		sei();
		return;
	}
	mode_t lmode=mode;
	wmode_t lwmode=wmode;
	uint8_t lbitp=bitp;
	uint8_t lactbit=actbit;

	DBG(0x03);
	DIS_TIMER();
	if (CHK_INT_EN()) {
		// reset pulse?
		uint8_t p = !!(OWPIN&(1<<ONEWIREPIN));
		if (p==0) { 
			DBG(0x05);
			DBG_C('R');
			lmode=OWM_IN_RESET;  //wait for rising edge
			lwmode=OWW_NO_WRITE;
			SET_RISING(); 
			CLEAR_LOW();
			//DBG_C('R');
		}
		DBG(0x06);
	} else
	switch (lmode) {
	case OWM_IDLE:
		DBG(0x07);
		break;
	case OWM_SLEEP: // should have been caught by CHK_INT_EN() above
		DBG(0x08);
		DBGS_P("\nChk Sleep!\n");
		break;
	case OWM_IN_RESET: // should not happen here
		DBG(0x09);
		DBGS_P("\nChk Reset!\n");
		break;
	case OWM_AFTER_RESET:  //Time after reset is finished, now go to presence state
		DBG(0x0A);
		lmode=OWM_PRESENCE;
		SET_LOW();
		SET_TIMER(OWT_PRESENCE);
		DIS_OWINT();  // wait for presence is done
		EN_TIMER();
		break;
	case OWM_PRESENCE:
		DBG(0x0B);
		CLEAR_LOW();  //Presence is done, now wait for a command
		START_READING(OWM_READ_SELECT,8);
		xmode = OWX_SELECT;
		break;
	case OWM_READ:
	case OWM_READ_SELECT:
	case OWM_READ_COMMAND:
		DBG(0x0C);
		if(lbitp) {
			DBG(0x0D);
			//DBG_C(p ? 'B' : 'b');
			if (bitbuf) { // Set bit if line high 
				cbuf |= lbitp;
			} else {
				DBG(0x1D);
			}
			lbitp <<= 1;
			if (!lbitp) {
				DBG(0x0E);
				switch(lmode) {
				default:
					DBG(0x0F);
					break;
				case OWM_READ_COMMAND:
					DBG(0x10);
					cmdbuf = cbuf;
					xmode = OWX_COMMAND;
					START_READING(OWM_READ,8);
					break;
				case OWM_READ_SELECT:
					DBG(0x11);
					cmdbuf = cbuf;
					xmode = OWX_SELECT;
					switch(cbuf) {
#ifdef CONDITIONAL_SEARCH
					case 0xEC: // CONDITIONAL SEARCH
						/* 
						Prep for conditional search here. If we then terminate early,
						the bus will recover.
						*/
#endif
					case 0xF0: // SEARCH_ROM; handled in interrupt
						lmode = OWM_SEARCH_ZERO;
						bytep = 0;
						lbitp = 1;
						cbuf = ow_addr.addr[0];
						lactbit = cbuf&1;
						lwmode = actbit ? OWW_WRITE_1 : OWW_WRITE_0;
						break;
					case 0x55: // MATCH_ROM
						lmode = OWM_READ;
						lbitp = 1;
						cbuf = 0;
						break;
#ifdef SINGLE_DEVICE
					case 0xCC: // SKIP_ROM
						START_READING(OWM_READ_COMMAND,8);
						break;
					case 0x33: // READ_ROM
						XMIT_BYTE(ow_addr.addr[0]);
						break;
#endif
					default:
						break;
					}
					DBG(0x12);
				}
			} else
				lwmode = OWW_READ;
		} else {
			// Overrun!
			DBG(0x13);
			DBGS_P("\nRead OVR!\n");
			lmode = OWM_SLEEP;
		}
		break;
	case OWM_WRITE:
		DBG(0x14);
		if (lbitp) {
			DBG(0x15);
			lwmode = (cbuf & lbitp) ? OWW_WRITE_1 : OWW_WRITE_0;
			lbitp <<= 1;
		} else {
			DBG(0x16);
			lmode = OWM_IDLE;
			lwmode = OWW_NO_WRITE;
		}
		break;
	case OWM_SEARCH_ZERO:
		DBG(0x17);
		lmode = OWM_SEARCH_ONE;
		lwmode = lactbit ? OWW_WRITE_0 : OWW_WRITE_1;
		break;
	case OWM_SEARCH_ONE:
		DBG(0x18);
		lmode = OWM_SEARCH_READ;
		lwmode = OWW_READ;
		break;
	case OWM_SEARCH_READ:
		DBG(0x19);
		//DBG_C(p ? 'B' : 'b');
		if (bitbuf != lactbit) {  //check master bit
			DBG(0x1A);
			//DBG_C('d');
			lmode = OWM_SLEEP;  //not the same: go to sleep
			break;
		}
		DBG(0x1B);

		lbitp=(lbitp<<1);  //prepare next bit
		if (!lbitp) {
			DBG(0x1C);
			uint8_t lbytep = bytep;
			lbytep++;
			if (lbytep>=8) {
				DBG(0x1D);
				START_READING(OWM_READ_COMMAND,8);
				break;
			}
			DBG(0x1E);
			bytep=lbytep;
			lbitp=1;
			cbuf = ow_addr.addr[lbytep];
		}
		DBG(0x1F);
		lmode = OWM_SEARCH_ZERO;
		lactbit = !!(cbuf&lbitp);
		lwmode = lactbit ? OWW_WRITE_1 : OWW_WRITE_0;
		break;
	}
	if (pin_poll) DBG(0x20); else DBG(0x30);
	if (lmode == OWM_SLEEP) {
		DBG(0x21);
		DIS_TIMER();
	}
	DBG(0x22);
	if (lmode != OWM_PRESENCE) { 
		DBG(0x23);
		SET_TIMER(OWT_MIN_RESET-OWT_READLINE);  //OWT_READLINE around OWT_LOWTIME
		EN_OWINT();
	}
	if (pin_poll) DBG(0x24); else DBG(0x34);

	cli();
	if (pin_poll || (timer_poll != 1)) {
		DBG(0x25);
		DBGS_P("\nOVt ");
		DBGS_Y(timer_poll);
		DBGS_Y(pin_poll);
		DBGS_C(' ');
		DBGS_Y(mode);
		DBGS_Y(lmode);
		DBGS_C('\n');
		set_idle();
	} else {
		DBG(0x26);
		timer_poll = 0;
		mode=lmode;
		wmode=lwmode;
		bitp=lbitp;
		actbit=lactbit;
	}
	sei();
	DBG(0x2C);
}

// 1wire level change.
// Do this in assembler so that writing a zero is fast enough on 8MHz.
// The whole thing should take <40 cycles (5µs), and pull down 1wire in
// 12 cycles (1.5µs) which should be sufficient. TEST ME.
void INT0_vect(void) __attribute__((naked));
void INT0_vect(void) {
	// WARNING: No command in here may change the status register!
	volatile register uint8_t t asm ("r24");
	volatile register uint8_t z asm ("r25");
	asm("     push r24");
	asm("     push r25");
	DBGA(0x3F,t);
	t = wmode;
	// if wmode == OWW_WRITE_0
		z = OWW_WRITE_0;
		asm("     cpse %0,%1" :: "r"(z),"r"(t));
		asm("     rjmp .La");
		SET_LOW();
		DBGA(0x3E,t);
		t = 1;
		asm("     out %0, %1" :: "i"(((int)&GTCCR)-__SFR_OFFSET), "r"(t));
		t = ~(OWT_LOWTIME);
		TCNT_REG=t;
		asm("     rjmp .Lb");

	// otherwise wait for read. We do that also when writing a 1
	// because that leaves enough time for the timeout handler.
	// Otherwise, if the master is searching and another slaves writes 
	// a 0 but finishes earlier than expected, we'd be screwed.
		asm(".La:         ");
		DBGA(0x3D,t);
		t = 1;
		asm("     out %0, %1" :: "i"(((int)&GTCCR)-__SFR_OFFSET), "r"(t));
		t = ~(OWT_READLINE);
		TCNT_REG=t;
		asm(".Lb:         ");
	// end if

	// Now set pin_poll. This code will set it to 1 if it was zero, else 3.
	// We use "sbrc" (skip if bit0 is clear) to save on comparisons,
	// which would require saving the condition register.
	asm(".Lc:");
	t = pin_poll;
	asm("     ldi %0,1" : "=r"(z));
	asm("     sbrc %0,0" :: "r"(t), "r"(z));
	asm("     ldi %0,3" : "=r"(z));
	pin_poll = z;

	asm(".Lcx:");
	DBGA(0x3C,t);
	asm("     pop r25");
	asm("     pop r24");
	asm("     reti");
}

void real_PIN_INT(void)
{
	if(!pin_poll) return;
	DBG(0x10);
	if(timer_poll) {
		DBG(0x11);
		DBGS_P("\nOVp2 ");
		DBGS_Y(mode);
		DBGS_C('\n');
		set_idle();
		sei();
		return;
	}
	DBG(0x12);
	DIS_OWINT(); //disable interrupt, only in OWM_SLEEP mode it is active
	mode_t lmode = mode;

	switch (lmode) {
	case OWM_PRESENCE:
		DBG(0x13);
		DBGS_P("\nChk Presence!\n");
		break;
	case OWM_IDLE:
		DBG(0x14);
		DBGS_P("\nChk Idle!");
		DBGS_X(cbuf);
		DBGS_C('\n');
		set_idle();
		sei();
		/* fall thru */
	case OWM_SLEEP:
		DBG(0x22);
		SET_TIMER(OWT_MIN_RESET);
		EN_OWINT(); //any earlier falling edges will simply reset the timer
		EN_TIMER();
		break;
	//start of reading with falling edge from master, reading closed in timer isr
	case OWM_IN_RESET:  //rising edge of reset pulse
		DBG(0x16);
		SET_TIMER(OWT_RESET_PRESENCE);  //wait before sending presence pulse
		mode=OWM_AFTER_RESET;
		SET_FALLING();
		EN_OWINT();
		EN_TIMER();
		//DBG_C('r');
		break;
	case OWM_AFTER_RESET: // some other chip was faster, assert my own presence signal now
		DBG(0x17);
		cli(); // otherwise the code below will trigger
		SET_TIMER(0);
		EN_TIMER();
		break;
	default:
		DBG(0x18);
		if(wmode != OWW_NO_WRITE)
			EN_TIMER();
		break;
	}

	cli();
	if (timer_poll || (pin_poll != 1)) {
		DBG(0x29);
		DBGS_P("\nOVpin ");
		DBGS_Y(pin_poll);
		DBGS_Y(timer_poll);
		DBGS_C(' ');
		DBGS_Y(lmode);
		DBGS_Y(mode);
		DBGS_C('\n');
		set_idle();
	} else {
		DBG(0x1A);
		pin_poll = 0;
	}
	sei();
//	if (mode > OWM_PRESENCE)
//		DBG_T(1);
}

