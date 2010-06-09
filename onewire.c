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

#include <setjmp.h>

#include "features.h"
#include "onewire.h"

/* Basic bus state machine, coding should not be visible in applications
 *   as it may change (optimization etc.). If any of this is necessary
 *   in application code create an interface function!
 */
//  Bitmasks
#define S_RECV 		0x01		// all receiving states have this bit set
#define S_XMIT 		0x02		// all transmitting states have this bit set
#define S_MASK 		0x7F
#define S_XMIT2 	0x80		// flag to de-assert zero bit on xmit timeout
#define S_ROMCODE	0x10		// romcode state (like matchrom, readrom, searchrom)
#define S_OPCODE	0x20		// opcode state (implemented by the actual device)

//  initial states: >3 byte times
#define S_IDLE            (       0x00) // wait for Reset
#define S_RESET           (       0x04) // Reset seen
#define S_PRESENCEPULSE   (       0x08) // sending Presence pulse
//  selection opcode states: 1 byte times
#define S_RECEIVE_ROMCODE (S_RECV|S_ROMCODE|0x00) // reading selection opcode
#define S_MATCHROM        (S_RECV|S_ROMCODE|0x04) // select a known slave
#define S_READROM         (S_XMIT|S_ROMCODE|0x04) // single slave only!

#ifndef SKIP_SEARCH
#define S_SEARCHROM       (S_XMIT|S_ROMCODE|0x08) // search, step 1: send ID bit
#define S_SEARCHROM_I     (S_XMIT|S_ROMCODE|0x0C) // search, step 2: send inverted ID bit
#define S_SEARCHROM_R     (S_RECV|S_ROMCODE|0x08) // search, step 3: check what the master wants
#endif
//  opcode states: 1 bit time
#define S_RECEIVE_OPCODE  (S_RECV|S_OPCODE|0x00)	// reading real opcode
#define S_HAS_OPCODE      (       S_OPCODE|0x04)	// has real opcode, mainloop
#define S_CMD_RECV        (S_RECV|S_OPCODE|0x08)	// receive bytes
#define S_CMD_XMIT        (S_XMIT|S_OPCODE|0x08)	// send bytes
#define S_CMD_IDLE        (       S_OPCODE|0x08)	// do nothing

// 1wire interface
#if 0 // def __AVR__
// register optimization does not seem to work, these variables get corrupted
register u_char bitcount asm("r2");
register u_char transbyte asm("r3");
register u_char xmitlen asm("r4");
// global
register volatile u_char state asm("r5");
#else
/* everything is volatile here ? TODO */
static u_char bitcount;			// number of bits to receive or transmit
static u_char transbyte;		// shift buffer for xmit and recv
static u_char xmitlen;			// number of bytes to transmit
static volatile u_char state;
#endif

static unsigned char addr[8];
static jmp_buf end_out;

#ifdef DBGPIN
static char interest = 0;
#define DBG_IN() interest=1
#define DBG_OUT() interest=0
#define DBG_ON() if(interest) OWPORT |= (1<<DBGPIN)
#define DBG_OFF() if(interest) OWPORT &= ~(1<<DBGPIN)
#else
#define DBG_IN()
#define DBG_OUT()
#define DBG_ON()
#define DBG_OFF()
#endif


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
#if F_CPU > 12000000
	#define T_SAMPLE T_(15)-1
#elif F_CPU > 9600000
	#define T_SAMPLE T_(15)-2
#else
	#warning "This will probably only work for relatively slow masters!"
	#define T_SAMPLE T_(25)-1	// only tested for atmega32, works but out of specification!
#endif
#define T_XMIT T_(60)-4			// overhead (measured w/ scope on ATmega168)
#endif

// check timing setup, T_RESET depends on timer size (8bits for AVR)
#if (T_SAMPLE<1)
#error "Sample time too short, fix timing!"
#endif
#if (T_RESET>200)
#error "Reset slot is too wide, fix timing!"
#endif

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
	longjmp(end_out, 1);
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
	longjmp(end_out, 1);
}

/*!
 * waits for a receiving or transmitting state to complete.
 * It calls update_idle() while waiting. After completion it
 * tests for strange situations, like:
 *  - transmission completed, but xmitlen != 0
 *  - reception completed, but bitcount != 0
 *  - and some state error I currently don't comprehend. TODO
 *
 *  \note If it returns (and not longjmp's to end_out) it will
 *  still have interrupts globally disabled.
 *
 *  \note Debug output can still be distinguished between xmit and recv,
 *    as they are preceded by either '<' or '>'.
 */
static void wait_for_completion_and_check_for_errors(void)
{
	/* wait for receive or transmit to end, call background task while waiting */
	while(state & (S_RECV | S_XMIT))
		update_idle(bitcount);

	cli();
	if(!(state & S_OPCODE)) {
		sei();
		if(state != S_IDLE) {
			// a basic state (reset, presence pulse), but not idle
			if (state < S_ROMCODE)
				longjmp(end_out, 1);
			DBG_ONE("\nState error! ", state);
		}
		next_idle();
	}
	/* state does not S_XMIT, but xmitlen not 0 ! */
	if(xmitlen) {
		sei();
		DBG_ONE("\nXbuflen error! ", xmitlen);
		next_idle();
	}
	/* state does not S_RECV, but bitcount not 0 */
	if(bitcount) {
		sei();
		DBG_TWO("\nBitcount error! ", state, bitcount);
		next_idle();
	}
}

/*! called by xmit_bit() and xmit_byte() */
static inline void xmit_any(u_char val, u_char len)
{
	wait_for_completion_and_check_for_errors();

	/* set-up new transmit */
	transbyte = val;
	bitcount = len;
	state = S_CMD_XMIT | (state & S_XMIT2);
	DBG_X(val);
	sei();
	DBG_OFF();
}

/*! transmit a single bit (true or false) */
void xmit_bit(u_char val)
{
	DBG_C('<');	DBG_C('_');
	xmit_any(!!val, 1);
}

/*! transmit a byte */
void xmit_byte(u_char val)
{
	DBG_C('<');
	xmit_any(val, 8);
}

/*! returns true if not a receiving or transmitting state active
 * TODO: never used !
 */
u_char rx_ready(void)
{
	return !(state & (S_RECV | S_XMIT));
}

/*! called by recv_bit() and recv_byte() */
static inline void recv_any(u_char len)
{
	wait_for_completion_and_check_for_errors();

	/* set-up new reception */
	bitcount = len;
	state = S_CMD_RECV | (state & S_XMIT2);
	sei();
	DBG_OFF();
}

/*! receives a single bit */
void recv_bit(void)
{
	DBG_C('>'); DBG_C('_');
	recv_any(1);
}
/*! receives a byte */
void recv_byte(void)
{
	DBG_C('>');
	recv_any(8);
}

/*!
 * waits for receiving states to complete (calling
 * the 'background task' via update_idle()
 * skips out on all but ???? TODO
 */
static u_char recv_any_in(void)
{
	while(state & S_RECV)
		update_idle(bitcount);

	if ((state & S_MASK) != S_CMD_IDLE)
		longjmp(end_out, 1);

	return transbyte;
}
/*! return a bit received (start receiver with recv_bit())
 * \note currently never called
 */
u_char recv_bit_in(void)
{
	u_char byte;

	byte = ((recv_any_in() & 0x80) != 0);
	DBG_X(byte);
	return byte;
}
/*! return a byte received (start receiver with recv_byte()) */
u_char recv_byte_in(void)
{
	u_char byte;

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
		DBG_P(">idle:"); DBG_X(state); DBG_P(" b");
		DBG_N(xmitlen);	DBG_N(bitcount); DBG_C('\n');
		state = S_IDLE;
	}
	DBG_OFF();
	DBG_OUT();

	bitcount = 0;
	xmitlen = 0;

	clear_owtimer();
	unmask_owpin();
	owpin_hiz();
}

/*!
 *  calculate crc16: with seed r and byte x
 *   TODO not sure here: using the IBM-CRC-16 polynom x^16+x^15+x^2+x^0 used for i-buttons
 *
 *  this code is from owfs
 */
static u_char parity_table[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };
u_short crc16(u_short r, u_char x)
{
	u_short c = (x ^ (r & 0xFF));
	r >>= 8;
	if (parity_table[c & 0x0F] ^ parity_table[(c >> 4) & 0x0F])
		r ^= 0xC001;
	r ^= (c <<= 6);
	r ^= (c << 1);
	return r;
}


/*!
 * initialize the hardware:
 * cpu_setup:
 *  - the prescaler for the 1-wire timer
 *  - the 1-wire pin interrupt to be sensitive to any level change
 *  owpin_setup:
 *  - the 1-wire port pin to input
 *  - the port register to 0 (so only DDR has to be changed to output)
 *  get_ow_address:
 *  - setup the 1-wire address (8 bytes) (from eeprom or wherever it might be)
 *
 *  - calls init_state() to setup application specific code
 *  - initializes the debugging code
 */
int main(void)
{
	cpu_setup();
	init_debug();
	owpin_setup();

#ifdef DBGPIN
	OWPORT &= ~(1 << DBGPIN);
	OWDDR |= (1 << DBGPIN);
#endif
	// initialize time stamping code here, if any
	// initialize global address buffer
	// portability issue, this might be as simple as returning a pointer!
	get_ow_address(addr);

	state = S_IDLE; set_idle();
	init_state();		// init application-specific code

	// now go
	sei();
	DBG_ONE("\nInit done! Tsample=", T_SAMPLE);
	DBG_ONE("Treset=0x", T_RESET);
	DBG_ONE("Txmit=0x", T_XMIT);

	// save context to return to in case a command is either completed
	// or interrupted by a reset condition
	setjmp(end_out);
	unmask_owpin();

	while (1) {
#ifdef HAVE_UART
		/* portability problem, this might be really slow and quite fast
		 * depending on processor speed, this is an 'alive' ticker
		 * we could use the overflow of the 1-wire timer (at 8bit and ~1-10usec resolution)
		 * = 250us .. 2.5ms, counting to 400-4000 overflows
		 */
		volatile unsigned long long int x;
		DBG_C('/');
		for(x=0;x<100000ULL;x++)
#endif
		 {

			if((state & S_MASK) == S_HAS_OPCODE)
				do_command(transbyte);

			// RESET processing takes > 12 bit times, plus one byte.
			update_idle((state == S_IDLE) ? 20 : (state < 0x10) ? 8 : bitcount);
		}
	}
}

/* ISRs cannot be interrupted, so this saves 34 bytes on gcc 4.3.3 */
#ifdef __AVR__
  #define state (*(u_char *) &state)
#endif

// Timer interrupt routine
OW_TIMER_ISR()
{
	u_char pin, st = state & S_MASK;
	pin = owpin_value();		// sample immediately
	clear_owtimer();
	//DBG_C(pin ? '!' : ':');
	if (state & S_XMIT2) {
		// de-assert a '0' on the pin set in pin interrupt
		state &= ~S_XMIT2;
		unmask_owpin();
		owpin_hiz();
		//DBG_C('x');
		goto end;
	}
	if (st == S_RESET) {       // send a presence pulse
		owpin_low();
		state = S_PRESENCEPULSE;
		set_owtimer(T_PRESENCE);
		DBG_C('P');
		goto end;
	}
	if (st == S_PRESENCEPULSE) {
		owpin_hiz();					// Presence pulse done
		state = S_RECEIVE_ROMCODE;		// wait for command
		DBG_C('O');
		goto end;
	}
	if (!(st & S_RECV)) { // any other non-receive situation is a NO-OP;
		DBG_ONE("T ", st);	// this really should not happen anyway
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
		DBG_X(transbyte); DBG_C(':');
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
			//DBG_C('?'); DBG_X(transbyte); DBG_C(' ');
		}
		else
#endif
		{
			DBG_P("::Unknown "); DBG_X(transbyte);
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
			//DBG_C('?'); DBG_X(transbyte);
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
	/* if the receiver is done (bitcount bits received)
	 * state is changed to S_CMD_IDLE and the application
	 * must set-up the new state within about 40usec!
	 */
	if (st == S_CMD_RECV) {
		bitcount = 0;			// fix bitcount (was set to 8 above)
		state = S_CMD_IDLE;
		DBG_ON();
		goto end;
	}
	// no idea what to do here, probably the main program was too slow
	{
		DBG_P("? rcv s"); DBG_X(state);
		set_idle();
	}
end:;
	DBG_OFF();
}


/*! Any 1wire level change */
OW_PINCHANGE_ISR()
{
	/* all but XMIT2 bit */
	u_char st = state & S_MASK;

	if (owpin_value()) {
		/* low to high transition */
		//DBG_C('^');

		/* check the length of the pulse, smaller than timeout
		 * and larger than T_RESET, TIMEOUT is 0xF0, this is
		 * a reset pulse.
		 */
		if (TCNT0 > T_RESET && (TCNT0 < 0xF0 || st == S_IDLE)) {
			set_owtimer(T_PRESENCEWAIT);
			state = S_RESET;
			bitcount = 8;
			xmitlen = 0;
			DBG_P("\nR");
			DBG_OUT();
		} // else do nothing special; the timer will read the state
		return;
	}
	//DBG_C('_');
	/*
	 * some transmitting state
	 */
	if (st & S_XMIT) {
		if ((transbyte & 0x01) == 0) {
			mask_owpin();
			owpin_low();		// send zero
			set_owtimer(T_XMIT);
			state |= S_XMIT2;
		} else
			clear_owtimer();

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
		/* if transmitter is done state is set to S_CMD_IDLE
		 * and the application must set-up the next state
		 * within about 1 bit-time (60usec)
		 */
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
		/* application did not continue with either S_CMD_RECV or S_CMD_XMIT */
		DBG_C('x');
		set_idle();
	}
	else if (st == S_IDLE) {		   // first 1-0 transition
		clear_owtimer();
	}
	/* any receiving state, synchronize sampling with h2l transition */
	else if (state & S_RECV) {
		set_owtimer(T_SAMPLE);
	}
	/* some other device sending a presence pulse */
	else if (st == S_RESET) {
		DBG_C('p');
		state = S_RECEIVE_ROMCODE;
		clear_owtimer();
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


