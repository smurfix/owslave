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

/* Based on work published at http://www.mikrocontroller.net/topic/44100 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <setjmp.h>

#define MAIN
#include "onewire.h"
#include "features.h"

#define PRESCALE 64
#define USEC2TICKS(x) (x/(F_CPU/PRESCALE))

#undef  EMU_18b20 // temperature
#define EMU_2423  // counter

// 1wire interface
static uint8_t bitcount, transbyte;
#define vbitcount *(volatile uint8_t *)&bitcount
static uint8_t xmitlen;
static unsigned char addr[8];

static jmp_buf end_out;


#ifdef __AVR_ATtiny13__
#define OWPIN PINB
#define OWPORT PORTB
#define OWDDR DDRB
#define ONEWIREPIN 1		 // INT0

#elif defined(__AVR_ATtiny84__)
#define OWPIN PINB
#define OWPORT PORTB
#define OWDDR DDRB
#define ONEWIREPIN 2		 // INT0

#elif defined (__AVR_ATmega8__)
#define OWPIN PIND
#define OWPORT PORTD
#define OWDDR DDRD
#define ONEWIREPIN 2		// INT0

#elif defined (__AVR_ATmega168__)
#define OWPIN PIND
#define OWPORT PORTD
#define OWDDR DDRD
#define ONEWIREPIN 2		// INT0
//#define DBGPIN 3		// debug output

#else
#error Pinout for your CPU undefined
#endif

#define T_(c) ((F_CPU/64)/(1000000/c))
#define T_PRESENCE T_(120)-5
#define T_PRESENCEWAIT T_(20)
#define T_SAMPLE T_(15)
#define T_XMIT T_(60)-5		// overhead (measured w/ scope on ATmega168)
#define T_RESET_ T_(400)        // timestamp for sampling
#define T_RESET (T_RESET_-T_SAMPLE)
#if (T_RESET>200)
#error Reset slot is too wide, fix timing
#endif

#define BAUDRATE 57600

#ifdef DBGPIN
static char interest = 0;
#define DBG_IN() interest=1
#define DBG_OUT() interest=0
#define DBG_ON() if(interest) OWPORT |= (1<<DBGPIN)
#define DBG_OFF() if(interest) OWPORT &= ~(1<<DBGPIN)
#else
#define DBG_IN() do { } while(0)
#define DBG_OUT() do { } while(0)
#define DBG_ON() do { } while(0)
#define DBG_OFF() do { } while(0)
#endif

// stupidity
#ifndef TIMER0_OVF_vect
#  define TIMER0_OVF_vect TIM0_OVF_vect
#endif


// Initialisierung der Hardware
void setup(void)
{
	unsigned char i;
#ifdef __AVR_ATtiny13__
	CLKPR = 0x80;	 // Prepare to ...
	CLKPR = 0x00;	 // ... set to 9.6 MHz

	TCCR0A = 0;
	TCCR0B = 0x03;	// Prescaler 1/64

	MCUCR |= (1 << ISC00);		  // Interrupt on both level changes

#elif defined (__AVR_ATtiny84__)
	CLKPR = 0x80;	 // Prepare to ...
	CLKPR = 0x00;	 // ... set to 8.0 MHz

	MCUCR |= (1 << ISC00);		  // Interrupt on both level changes
	
#elif defined (__AVR_ATmega8__)
	// Clock is set via fuse
	// CKSEL = 0100;   Fuse Low Byte Bits 3:0

	TCCR0 = 0x03;	// Prescaler 1/64

	MCUCR |= (1 << ISC00);		  // Interrupt on both level changes

#elif defined (__AVR_ATmega168__)
	// Clock is set via fuse

	TCCR0A = 0;
	TCCR0B = 0x03;	// Prescaler 1/64

	EICRA = (1<<ISC00); // interrupt of INT0 (pin D2) on both level changes

#else
#error Not yet implemented
#endif

	OWPORT &= ~(1 << ONEWIREPIN);
	OWDDR &= ~(1 << ONEWIREPIN);

#ifdef DBGPIN
	OWPORT &= ~(1 << DBGPIN);
	OWDDR |= (1 << DBGPIN);
#endif
#ifdef HAVE_TIMESTAMP
	TCCR1A = 0;
	TCCR1B = (1<<ICES1) | (1<<CS10);
	TIMSK1 &= ~(1<<ICIE1);
	TCNT1 = 0;
#endif

	// Get 64bit address from EEPROM
	while(EECR & (1<<EEPE));	 // Wait for EPROM circuitry to be ready
	for (i=8; i;) {
		i--;
		/* Set up address register */
		EEARL = 7-i;			   // set EPROM Address
		/* Start eeprom read by writing EERE */
		EECR |= (1<<EERE);
		/* Return data from data register */
		addr[i] =  EEDR;
	}

	// init application-specific code
	init_state();

	IFR |= (1 << INTF0);
	IMSK |= (1 << INT0);

#ifdef HAVE_UART
	uart_init(UART_BAUD_SELECT(BAUDRATE,F_CPU));
#endif
}

static inline void set_timer(int timeout)
{
	//DBG_C('T');
	//DBG_X(timeout);
	//DBG_C(',');
	TCNT0 = ~timeout;
	TIFR0 |= (1 << TOV0);
	TIMSK0 |= (1 << TOIE0);
}

static inline void clear_timer(void)
{
	//DBG_C('t');
	TCNT0 = 0;
	TIMSK0 &= ~(1 << TOIE0);	   // turn off the timer IRQ
}

void next_idle(void)
{
	if(state != S_IDLE)
		DBG_P("\nj_out\n");
	set_idle();
	longjmp(end_out,1);
}

void next_command(void)
{
	if ((state & S_MASK) != S_CMD_IDLE) {
		DBG_P("\nc_out\n");
		set_idle();
	} else {
		bitcount = 8;
		state = S_RECEIVE_OPCODE | (state & S_XMIT2);
	}
	longjmp(end_out,1);
}

static void
xmit_any(uint8_t val, uint8_t len)
{
	DBG_IN();
	while(state & (S_RECV|S_XMIT))
		update_idle(vbitcount);
	cli();
	if(!(state & 0x20)) {
		sei();
		if(state != S_IDLE) {
			DBG_P("\nState error xmit! ");
			DBG_X(state);
			DBG_C('\n');
		}
		next_idle();
	}
	if(xmitlen) {
		sei();
		DBG_P("\nXbuflen error xmit! ");
		DBG_X(xmitlen);
		DBG_C('\n');
		next_idle();
	}
	if(bitcount) {
		sei();
		DBG_P("\nBitcount error xmit! ");
		DBG_X(state);
		DBG_C(',');
		DBG_X(bitcount);
		DBG_C('\n');
		next_idle();
	}
	transbyte = val;
	bitcount = 8;
	state = S_CMD_XMIT | (state & S_XMIT2);
	DBG_X(val);
	sei();
	DBG_OFF();
}
void xmit_bit(uint8_t val)
{
	DBG_C('<');
	DBG_C('_');
	xmit_any(!!val,1);
}

void xmit_byte(uint8_t val)
{
	DBG_C('<');
	xmit_any(val,8);
}

uint8_t rx_ready(void)
{
	return !(state & (S_RECV|S_XMIT));
}

static void
recv_any(uint8_t len)
{
	while(state & (S_RECV|S_XMIT))
		update_idle(vbitcount);
	cli();
	if(!(state & 0x20)) {
		sei();
		if (state != S_IDLE) {
			DBG_P("\nState error recv! ");
			DBG_X(state);
			DBG_C('\n');
		}
		next_idle();
	}
	if(xmitlen) {
		sei();
		DBG_P("\nXbuflen error recv! ");
		DBG_X(xmitlen);
		DBG_C('\n');
		next_idle();
	}
	if(bitcount) {
		sei();
		DBG_P("\nBitcount error recv! ");
		DBG_X(bitcount);
		DBG_C('\n');
		next_idle();
	}
	bitcount = len;
	state = S_CMD_RECV | (state & S_XMIT2);
	sei();
	DBG_OFF();
	DBG_C('>');
}
static uint8_t recv_any_in(void)
{
	while(state & S_RECV)
		update_idle(vbitcount);
	if ((state & S_MASK) != S_CMD_IDLE)
		longjmp(end_out,1);
	return transbyte;
}
void recv_bit(void)
{
	recv_any(1);
	DBG_C('_');
}

void recv_byte(void)
{
	recv_any(8);
}
uint8_t recv_bit_in(void)
{
	uint8_t byte;
	byte = ((recv_any_in() & 0x80) != 0);
	DBG_X(byte);
	return byte;
}

uint8_t recv_byte_in(void)
{
	uint8_t byte;
	byte = recv_any_in();
	DBG_X(byte);
	return byte;
}


// this code is from owfs
static uint8_t parity_table[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };
uint16_t crc16(uint16_t r, uint8_t x)
{
	uint16_t c = (x ^ (r & 0xFF));
	r >>= 8;
	if (parity_table[c & 0x0F] ^ parity_table[(c >> 4) & 0x0F])
		r ^= 0xC001;
	r ^= (c <<= 6);
	r ^= (c << 1);
        return r;
}


// Main program
int main(void)
{
#ifdef HAVE_TIMESTAMP
	tbpos = sizeof(tsbuf)/sizeof(tsbuf[0]);
	uint16_t last_tb = 0;
#endif

	state = S_IDLE;
	setup();
	set_idle();

	// now go
	sei();
	DBG_P("\nInit done!\n");

	setjmp(end_out);
	while (1) {
#ifdef HAVE_UART
		volatile unsigned long long int x; // for 'worse' timing
		DBG_C('/');
		for(x=0;x<100000ULL;x++)
#endif
		 {
			if((state & S_MASK) == S_HAS_OPCODE)
				do_command(transbyte);

			// RESET processing takes > 12 bit times, plus one byte.
			update_idle((state == S_IDLE) ? 20 : (state < 0x10) ? 8 : vbitcount);
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
		}
	}
}

void set_idle(void)
{
	if(state != S_IDLE) {
		DBG_P(">idle:");
		DBG_X(state);
		DBG_P(" b");
		DBG_N(xmitlen);
		DBG_N(bitcount);
		DBG_NL();
		state = S_IDLE;
	}
	DBG_OFF();
	DBG_OUT();

	bitcount = 0;
	xmitlen = 0;

	clear_timer();
	IFR |= (1 << INTF0);		// ack+enable level-change interrupt, just to be safe
	IMSK |= (1 << INT0);
	OWDDR &= ~(1 << ONEWIREPIN);	// set to input
}

static void set_reset(void) {
	state = S_RESET;
	bitcount = 8;
	xmitlen = 0;
	DBG_C('\n');
	DBG_C('R');
	DBG_OUT();
}


/* ISRs cannot be interrupted, so this saves 80 bytes */
#define state *(uint8_t *)&state

// Timer interrupt routine
ISR (TIMER0_OVF_vect)
{
	uint8_t pin, st = state & S_MASK;
	pin = OWPIN & (1 << ONEWIREPIN);
	clear_timer();
	//DBG_C(pin ? '!' : ':');
	if (state & S_XMIT2) {
		state &= ~S_XMIT2;
		IFR |= (1 << INTF0);
		IMSK |= (1 << INT0);
		OWDDR &= ~(1 << ONEWIREPIN);	// set to input
		//DBG_C('x');
		goto end;
	}
	if (st == S_RESET) {       // send a presence pulse
		OWDDR |= (1 << ONEWIREPIN);
		state = S_PRESENCEPULSE;
		set_timer(T_PRESENCE);
		DBG_C('P');
		goto end;
	}
	if (st == S_PRESENCEPULSE) {
		OWDDR &= ~(1 << ONEWIREPIN);	// Presence pulse done
		state = S_RECEIVE_ROMCODE;		// wait for command
		DBG_C('O');
		goto end;
	}
	if (!(st & S_RECV)) { // any other non-receive situation is a NO-OP;
		DBG_C('T');       // this really should not happen anyway
		set_idle();
		goto end;
	}
#ifndef SKIP_SEARCH
	if (st == S_SEARCHROM_R) {
		//DBG_C((pin != 0) + '0');
		if (((transbyte & 0x01) == 0) == (pin == 0)) { // non-match => exit
			// remember that S_SEARCHMEM_I left the bit inverted
			DBG_C('-');
			//DBG_X(transbyte);
			set_idle();
			goto end;
		}
		//DBG_C(' ');
		transbyte >>= 1;
		state = S_SEARCHROM;
		if(!--bitcount) {
			bitcount = 8;
			if(xmitlen) {
				transbyte = addr[--xmitlen];
				DBG_C('?');
				DBG_X(transbyte);
			} else {
				DBG_ON();
				state = S_RECEIVE_OPCODE;
				DBG_C('C');
				DBG_OFF();
			}
		}
		goto end;
	}
#endif
	DBG_ON();
	transbyte >>= 1;
	if (pin)
		transbyte |= 0x80;
	DBG_OFF();
	if(--bitcount)
		goto end;
	bitcount = 8;

	if (st == S_RECEIVE_ROMCODE) {
		DBG_X(transbyte);
		DBG_C(':');
		if (transbyte == 0x55) {
			state = S_MATCHROM;
			xmitlen = 8;
		}
		else if (transbyte == 0xCC) {
			 // skip ROM; nothing to do, just wait for next command
			DBG_ON();
			state = S_RECEIVE_OPCODE;
			DBG_OFF();
			DBG_C('C');
		}
		else if (transbyte == 0x33) {
			state = S_READROM;
			xmitlen = 8;
			transbyte = addr[7];
		}
		else
#ifndef SKIP_SEARCH
		if (transbyte == 0xF0) {
			state = S_SEARCHROM;
			xmitlen = 7;
			transbyte = addr[7];
			DBG_C('?');
			DBG_X(transbyte);
			//DBG_C(' ');
		}
		else
#endif
		{
			DBG_P("::Unknown ");
			DBG_X(transbyte);
			set_idle();
		}
		goto end;
	}
	if (st == S_RECEIVE_OPCODE) {
		DBG_IN();
		DBG_ON();
		DBG_X(transbyte);
		bitcount = 0;
		state = S_HAS_OPCODE | (state & S_XMIT2);
		goto end;
	}
#ifndef SKIP_SEARCH
	if (st == S_SEARCHROM) {
		if (xmitlen) {
			transbyte = addr[--xmitlen];
			DBG_C('?');
			DBG_X(transbyte);
		}
		else {
			DBG_ON();
			state = S_RECEIVE_OPCODE;
			DBG_C('C');
			DBG_OFF();
		}
		goto end;
	}
#endif
	if (st == S_MATCHROM) {
		if (transbyte != addr[--xmitlen]) {
			DBG_C('M');
			set_idle();
			goto end;
		}
		if (xmitlen)
			goto end;
		DBG_ON();
		state = S_RECEIVE_OPCODE;
		DBG_C('C');
		DBG_OFF();
		goto end;
	}
	if (st == S_CMD_RECV) {
		bitcount = 0;
		state = S_CMD_IDLE;
		DBG_ON();
		goto end;
	}
	// no idea what to do here, probably the main program was too slow
	{
		DBG_P("? rcv s");
		DBG_X(state);
		set_idle();
	}
end:;
	DBG_OFF();
}


// 1wire level change
ISR (INT0_vect)
{
	uint8_t st = state & S_MASK;
	if (OWPIN & (1 << ONEWIREPIN)) {	 // low => high transition
		DBG_TS();
		//DBG_C('^');
#ifdef HAVE_TIMESTAMP
		TCCR1B &=~ (1<<ICES1);
#endif
		if (((TCNT0 < 0xF0)||(st == S_IDLE)) && (TCNT0 > T_RESET)) {	// Reset pulse seen
			set_timer(T_PRESENCEWAIT);
			set_reset();
		} // else do nothing special; the timer will read the state
		goto end;
	}
	//TIFR0 |= (1 << TOV0);                 // clear timer IRQ
	DBG_TS();
	//DBG_C('_');
#ifdef HAVE_TIMESTAMP
	TCCR1B |= (1<<ICES1);
#endif
	if (st & S_XMIT) {
		if ((transbyte & 0x01) == 0) {
			IMSK &= ~(1 << INT0);
			OWDDR |= (1 << ONEWIREPIN);	// send zero
			set_timer(T_XMIT);
			state |= S_XMIT2;
		} else
			clear_timer();

#ifndef SKIP_SEARCH
		if (st == S_SEARCHROM) {
			//DBG_C((transbyte & 0x01) + '0');
			transbyte ^= 0x01;
			state = S_SEARCHROM_I | (state & S_XMIT2);
			goto end;
		} else if (st == S_SEARCHROM_I) {
			//DBG_C((transbyte & 0x01) + '0');
			// note: we leave the bit flipped here
			// and check for equality later
			state = S_SEARCHROM_R | (state & S_XMIT2);
			goto end;
		}
#endif
		transbyte >>= 1;
		if (--bitcount)
			goto end;

		if (st == S_READROM) {
			if (xmitlen) {
				bitcount = 8;
				transbyte = addr[--xmitlen];
				DBG_C('<');
				DBG_X(transbyte);
			} else {
				DBG_ON();
				state = S_RECEIVE_OPCODE | (state & S_XMIT2);
				DBG_C('C');
				DBG_OFF();
			}
		}
		else if (st == S_CMD_XMIT) {
			state = S_CMD_IDLE | (state & S_XMIT2);
			DBG_ON();
		}
		else {
			DBG_P("\nxmit: bad state\n");
			set_idle();
		}
		goto end;
	}
	else if (st == S_CMD_IDLE) {
		DBG_C('x');
		set_idle();
	}
	else if (st == S_IDLE) {		   // first 1-0 transition
		clear_timer();
	}
	else if (state & S_RECV) {
		set_timer(T_SAMPLE);
	}
	else if (st == S_RESET) {		   // somebody else sends their Presence pulse
		state = S_RECEIVE_ROMCODE;
		clear_timer();
	}
	else if (st == S_PRESENCEPULSE)
		;   // do nothing, this is our own presence pulse
	else {
		DBG_C('c');
		set_idle();
	}		
end:;
}


