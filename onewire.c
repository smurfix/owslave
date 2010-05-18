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

#include "onewire.h"
#include "features.h"

// 1wire interface
static uint8_t bitcount, transbyte;
// this actually increases the codesize by 6 bytes!
#define vbitcount *(volatile uint8_t *)&bitcount
static uint8_t xmitlen;
static unsigned char addr[8];
static jmp_buf end_out;

// global
volatile uint8_t state;

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

#elif defined (__AVR_ATmega168__) || defined (__AVR_ATmega32__)
#define OWPIN PIND
#define OWPORT PORTD
#define OWDDR DDRD
#define ONEWIREPIN 2		// INT0
//#define DBGPIN 3		// debug output
#else
#error "Pinout for your CPU undefined"
#endif

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

/*!
 * initialize the hardware, mainly the processors
 *  - the prescaler for timer 0
 *  - the int 0 pin interrupt to be sensitive to any level change
 *  - the 1-wire port pin to input
 *  - the port register to 0 (so only DDR has to be changed to output)
 *  - reads the 1-wire address (8 bytes) from eeprom address 0 in reversed order
 *  - calls init_state() to setup application specific code
 *  - and if HAVE_UART initializes the uart.
 *  - possibly defines the PRESCALER and BAUDRATE uP specific
 *     ... defaults are 64 and 57600
 */
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
#elif  defined (__AVR_ATmega32__)
#define BAUDRATE 38400
	// Clock is set via fuse to 8MHz
	TCCR0 = 0x03;	// Prescaler 1/64

	MCUCR |= (1 << ISC00);		  // Interrupt on both level changes

#elif defined (__AVR_ATmega168__)
	// Clock is set via fuse

	TCCR0A = 0;
	TCCR0B = 0x03;	// Prescaler 1/64

	EICRA = (1<<ISC00); // interrupt of INT0 (pin D2) on both level changes
#else
#error "Not yet implemented"
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

#ifndef PRESCALE
#define PRESCALE 64
#endif

/* macros to create timeout values in usec, e.g.
 *   8MHz cpu clock, prescaled with 64 -> 8 usec per timer tick
 *   120usec equals 15 ticks (8000000/64)/(1000000/120) = (8*120)/64 = 15
 *
 *   for the above example:
 *   T_PRESENCE = 10
 *   T_PRESENCEWAIT = 2.5 -> 2
 *   T_RESET_ = (8*400)/64 = 50
 *   T_RESET = 50-1 = 49
 *
 *   fallback values for 8MHz are
 *   T_SAMPLE = (8*25)/64-1 = 1
 *   T_XMIT = (8*60)/64-5 = 7.8..-5 ->7-5=2
 *
 */
#define T_(c) ((F_CPU/PRESCALE)/(1000000/c))
#define T_PRESENCE T_(120)-5
#define T_PRESENCEWAIT T_(20)
#define T_RESET_ T_(400)        // timestamp for sampling
#define T_RESET (T_RESET_-T_SAMPLE)

/* these are critical and should be set-up individually
 * ... the fallbacks may be invalid
 */
#ifndef T_SAMPLE
#if F_CPU > 9600000
	#define T_SAMPLE T_(15)-2	// overhead
#else
	#define T_SAMPLE T_(25)-1	// only tested for atmega32
#endif
#define T_XMIT T_(60)-5			// overhead (measured w/ scope on ATmega168)
#endif

// check timing setup
#if (T_SAMPLE<1)
#error "Sample time too short, fix timing"
#endif
#if (T_RESET>200)
#error "Reset slot is too wide, fix timing"
#endif

// 57600 and above did not work on atmega 32
#ifndef BAUDRATE
#define BAUDRATE 57600
#endif

/*! set timer 0 to a value that will overflow in timeout ticks */
static inline void set_timer(int timeout)
{
	//DBG_C('T');
	//DBG_X(timeout);
	//DBG_C(',');
	TCNT0 = ~timeout;				// overrun at 0xFF
	TIFR0 |= (1 << TOV0);
	TIMSK0 |= (1 << TOIE0);
}

/* set timer to 0, will overflow in FF ticks */
static inline void clear_timer(void)
{
	//DBG_C('t');
	TCNT0 = 0;
	TIMSK0 &= ~(1 << TOIE0);	   // turn off the timer IRQ
}

/*
 * functions below are called by the command level
 * (application specific code) and not directly from the
 * interrupt based state machine
 */

/*! called by recv_any() and xmit_any() to end command processing */
void next_idle(void)
{
	if(state != S_IDLE)
		DBG_P("\nj_out\n");
	set_idle();
	longjmp(end_out,1);
}

/*! currently not used at all */
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

/*! called from xmit_bit() and xmit_byte()
 *   it waits calling the 'background task' via update_idle()
 *   for receive or transmit states to complete.
 *   error handling is done here as well -> next_idle()
 */
static void xmit_any(uint8_t val, uint8_t len)
{
	while(state & (S_RECV|S_XMIT))
		update_idle(vbitcount);
	cli();
	if(!(state & 0x20)) {
		sei();
		if(state != S_IDLE) {
			if (state < 0x10)
				longjmp(end_out,1);
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

/*! transmit a single bit (true or false) */
void xmit_bit(uint8_t val)
{
	DBG_C('<');
	DBG_C('_');
	xmit_any(!!val,1);
}

/*! transmit a byte */
void xmit_byte(uint8_t val)
{
	DBG_C('<');
	xmit_any(val,8);
}

/*! returns true if not a receiving or transmitting
 * state active
 */
uint8_t rx_ready(void)
{
	return !(state & (S_RECV|S_XMIT));
}

/*! called from recv_bit() and recv_byte()
 *   it waits calling the 'background task' via update_idle()
 *   for receive or transmit states to complete.
 *   error handling is done here as well -> next_idle()
 */
static void recv_any(uint8_t len)
{
	while(state & (S_RECV|S_XMIT))
		update_idle(vbitcount);
	cli();
	if(!(state & 0x20)) {
		sei();
		if (state != S_IDLE) {
			if (state < 0x10)
				longjmp(end_out,1);
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

/*! receives a single bit */
void recv_bit(void)
{
	recv_any(1);
	DBG_C('_');
}
/*! receives a byte */
void recv_byte(void)
{
	recv_any(8);
}

/*!
 * waits for receiving states to complete (calling
 * the 'background task' via update_idle()
 * skips out on all but ???? TODO
 * 		called by recv_bit_in() and recv_byte_in()
 */
static uint8_t recv_any_in(void)
{
	while(state & S_RECV)
		update_idle(vbitcount);
	if ((state & S_MASK) != S_CMD_IDLE)
		longjmp(end_out,1);
	return transbyte;
}
/*! TODO */
uint8_t recv_bit_in(void)
{
	uint8_t byte;
	byte = ((recv_any_in() & 0x80) != 0);
	DBG_X(byte);
	return byte;
}
/*! TODO */
uint8_t recv_byte_in(void)
{
	uint8_t byte;
	byte = recv_any_in();
	DBG_X(byte);
	return byte;
}

/*!
 * called from command level or via next_command() and next_idle()
 *  mainly resets the state machine, the timer and the pin.
 */
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

/*!
 *  calculate crc16: with seed r and byte x
 *   TODO not sure here: using the IBM-CRC-16 polynom x^16+x^15+x^2+x^0 used for i-buttons
 *
 *  this code is from owfs
 */
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

/*! called by the state machine when reset pulse is seen ! */
static void set_reset(void) {
	state = S_RESET;
	bitcount = 8;
	xmitlen = 0;
	DBG_C('\n');
	DBG_C('R');
	DBG_OUT();
}


/* ISRs cannot be interrupted, so this saves 50 bytes on gcc 4.3.3 */
#define state *(uint8_t *)&state

// Timer interrupt routine
ISR (TIMER0_OVF_vect)
{
	uint8_t pin, st = state & S_MASK;
	pin = OWPIN & (1 << ONEWIREPIN);
	clear_timer();
	//DBG_C(pin ? '!' : ':');
	if (state & S_XMIT2) {
		// de-assert a '0' on the pin set in pin interrupt
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
				//DBG_C('?');
				//DBG_X(transbyte);
			} else {
				state = S_RECEIVE_OPCODE;
				DBG_C('C');
			}
		}
		goto end;
	}
#endif
	/* sample incoming data 8bits! */
	transbyte >>= 1;
	if (pin)
		transbyte |= 0x80;
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
			state = S_RECEIVE_OPCODE;
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
			//DBG_C('?');
			//DBG_X(transbyte);
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
		DBG_X(transbyte);
		bitcount = 0;
		state = S_HAS_OPCODE | (state & S_XMIT2);
		goto end;
	}
#ifndef SKIP_SEARCH
	if (st == S_SEARCHROM) {
		if (xmitlen) {
			transbyte = addr[--xmitlen];
			//DBG_C('?');
			//DBG_X(transbyte);
		}
		else {
			state = S_RECEIVE_OPCODE;
			DBG_C('C');
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
		state = S_RECEIVE_OPCODE;
		DBG_C('C');
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


/*! Any 1wire level change */
ISR (INT0_vect)
{
	/* all but XMIT2 bit */
	uint8_t st = state & S_MASK;

	if (OWPIN & (1 << ONEWIREPIN)) {
		/* low to high transition */
		DBG_TS();
		//DBG_C('^');
#ifdef HAVE_TIMESTAMP
		TCCR1B &=~ (1<<ICES1);
#endif
		/* check the length of the pulse, smaller than timeout
		 * and larger than T_RESET, TIMEOUT is 0xF0, this is
		 * a reset pulse.
		 */
		if (((TCNT0 < 0xF0) || (st == S_IDLE)) && (TCNT0 > T_RESET)) {
			set_timer(T_PRESENCEWAIT);
			set_reset();
		} // else do nothing special; the timer will read the state
		return;
	}
	//TIFR0 |= (1 << TOV0);                 // clear timer IRQ
	DBG_TS();
	//DBG_C('_');
#ifdef HAVE_TIMESTAMP
	TCCR1B |= (1<<ICES1);
#endif
	/*
	 * some transmitting state
	 */
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
			return;
		} else if (st == S_SEARCHROM_I) {
			//DBG_C((transbyte & 0x01) + '0');
			// note: we leave the bit flipped here
			// and check for equality later
			state = S_SEARCHROM_R | (state & S_XMIT2);
			return;
		}
#endif
		transbyte >>= 1;
		if (--bitcount)
			return;

		if (st == S_READROM) {
			if (xmitlen) {
				bitcount = 8;
				transbyte = addr[--xmitlen];
				DBG_C('<');
				DBG_X(transbyte);
			} else {
				state = S_RECEIVE_OPCODE | (state & S_XMIT2);
				DBG_C('C');
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
	}
	/* non-transmitting states */
	else if (st == S_CMD_IDLE) {
		DBG_C('x');
		set_idle();
	}
	else if (st == S_IDLE) {		   // first 1-0 transition
		clear_timer();
	}
	/* any receiving state, synchronize sampling with h2l transition */
	else if (state & S_RECV) {
		set_timer(T_SAMPLE);
	}
	/* some other device sending a presence pulse */
	else if (st == S_RESET) {
		state = S_RECEIVE_ROMCODE;
		clear_timer();
	}
	else if (st == S_PRESENCEPULSE)
		/* do nothing, this is our own presence pulse
		 * which actually is not possible as we hold down
		 * the 1-wire pin!
		 */
		;
	else {
		DBG_C('c');
		set_idle();
	}		
}


