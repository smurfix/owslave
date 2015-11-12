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

#include "onewire_internal.h"
#include <avr/eeprom.h>
#include <string.h> // for memset

ow_addr_t ow_addr;

volatile uint8_t bitp;  // mask of current bit
volatile uint8_t bytep; // position of current byte
volatile uint8_t cbuf;  // char buffer, current byte to be (dis)assembled
volatile xmode_t xmode;
volatile wmode_t wmode;
volatile uint8_t actbit; // current bit. Keeping this saves 14bytes ROM

void
onewire_init(void)
{
#ifdef __AVR_ATtiny13__
	TCCR0A = 0;
	TCCR0B = 0x03;	// Prescaler 1/64

#elif defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)

#elif defined(__AVR_ATtiny84__)

#elif defined (__AVR_ATmega8__)
	TCCR0 = 0x03;	// Prescaler 1/64

#elif defined (__AVR_ATmega168__) || defined (__AVR_ATmega88__) || defined(__AVR_ATmega328__)
#ifdef ONEWIRE_USE_T2
	TCCR2A = 0;
	TCCR2B = 0x03;	// Prescaler 1/32
#else
	TCCR0A = 0;
	TCCR0B = 0x03;	// Prescaler 1/64
#endif

#else
#error "Timer/IRQ setup for your CPU undefined"
#endif

	ONEWIRE_PORT &= ~ONEWIRE_PBIT;
	ONEWIRE_DDR &= ~ONEWIRE_PBIT;

	if(!cfg_read(owid, ow_addr.ow_addr))
	{
		ow_addr.ow_addr.type = 0xF0;
		memset(ow_addr.ow_addr.serial,0,6);
		ow_addr.ow_addr.crc = 0x44;
	}

	ONEWIRE_IFR |= ONEWIRE_IFBIT;
	ONEWIRE_IER |= ONEWIRE_IFBIT;

	set_idle();
}

#ifdef ONEWIRE_DEBUG
void next_idle(char reason)
#else
void _next_idle(void)
#endif
{
	DBG(0x2D);
	DBG_C('I');
	DBG_C(reason);
	if(mode > OWM_PRESENCE) {
		set_idle();
	}
	DBG(0x2D);
	go_out();
}

void next_command(void)
{
	wait_complete('n');
	start_reading(8);
	//DBG_P(".e4");

	xmode = OWX_COMMAND;
	DBG(0x2B);
	go_out();
}

void _wait_complete(void)
{
//	if(bitp || (wmode != OWW_NO_WRITE))
//		DBG_C(c);
	while(1) {
		if (mode < OWM_IDLE) {
//			DBG_P("s5");
			DBG(0x29);
			next_idle('m');
		}
		if(!bitp && (wmode == OWW_NO_WRITE)) {
			//DBG_OFF();
			return;
		}
		uart_poll();
		update_idle((mode == OWM_IDLE || bitp < 0x80) ? 8 : 1);
	}
}

static inline void
xmit_any(uint8_t val, uint8_t len)
{
	wait_complete('w');
	cli();
	if(mode == OWM_READ || mode == OWM_IDLE)
		mode = OWM_WRITE;
	if (mode != OWM_WRITE || xmode < OWX_RUNNING) {
		// DBG_P("\nErr xmit ");
		DBG(0x28);
		next_idle('x');
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
	while(mode == OWM_WRITE)
		wait_complete('J');
	cli();
	if(mode == OWM_IDLE)
		mode = OWM_READ;
	if (mode != OWM_READ || xmode < OWX_RUNNING) {
		DBG_P("\nState error recv! ");
		DBG_X(mode);
		DBG_C('\n');
		DBG(0x24);
		next_idle('s');
	}
	bitp = 1 << (8-len);
	cbuf = 0;
	sei();
	//DBG_OFF();
}


uint8_t
recv_any_in(void)
{
	wait_complete('i');
	if (mode != OWM_READ) {
		DBG_P(".e2");
		DBG(0x2A);
		go_out();
	}
	mode = OWM_IDLE;
	return cbuf;
}
#ifdef NEED_BITS
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

uint16_t recv_bytes_crc(uint16_t crc, uint8_t *buf, uint8_t len)
{
	uint8_t val;

	while(len--) {
		val = recv_byte_in();
		*buf++ = val;
		if (len)
			recv_byte();
		crc = crc16(crc, val);
	}
	return crc;
}


static inline void do_select(uint8_t cmd)
{
	uint8_t i;
#ifdef CONDITIONAL_SEARCH
	char cond;
#endif

	DBG_C('S');
	switch(cmd) {
#if defined(CONDITIONAL_SEARCH)
	case 0xEC: // CONDITIONAL SEARCH
		cond = condition_met();
#ifdef HAVE_WATCHDOG
		wdt_reset();
#endif
		if (!cond) {
			DBG(0x23);
			next_idle('c');
		}
		/* FALL THRU */
#endif // conditional
	case 0xF0: // SEARCH_ROM; handled in interrupt
		DBG_C('s');
		mode = OWM_SEARCH_ZERO;
		bytep = 0;
		bitp = 1;
		cbuf = ow_addr.addr[0];
		actbit = cbuf&1;
		wmode = actbit ? OWW_WRITE_1 : OWW_WRITE_0;
		return;
	case 0x55: // MATCH_ROM
		DBG_C('S'); DBG_C('m');
		recv_byte();
		for (i=0;;i++) {
			uint8_t b = recv_byte_in();
			if (b != ow_addr.addr[i]) {
				DBG(0x27);
				next_idle('n');
			}
			if (i < 7)
				recv_byte();
			else
				break;
		}
		//DBG_C('m');
		next_command();
#ifdef SINGLE_DEVICE
	case 0xCC: // SKIP_ROM
		DBG_C('k');
		next_command();
	case 0x33: // READ_ROM
		DBG_C('r');
		for (i=0;i<8;i++)
			xmit_byte(ow_addr.addr[i]);
		DBG(0x26);
		next_idle('r');
#endif
	default:
		DBG_C('?');
		DBG_X(cmd);
		DBG_C(' ');
		DBG(0x25);
		next_idle('u');
	}
}

/**
 * The reason for splitting onewire_poll() into two functions
 * (and for the OS_task attribute) is that otherwise, AVR-GCC
 * will use a heap of registers to put all my debugging constants
 * into separate registers before entering the loop. That is
 * completely unnecesary and eats a lot of time.
 */
char _onewire_poll(void) __attribute__((OS_task));
char _onewire_poll(void) {
	cli();
	if(!bitp) {
		DBG(0x13);
		xmode_t lxmode = xmode;
		sei();
		if(lxmode == OWX_SELECT) {
			DBG(cbuf);
			DBG(0x17);
			xmode = OWX_RUNNING;
			do_select(cbuf);
		}
		else if(lxmode == OWX_COMMAND) {
			DBG(cbuf);
			DBG(0x1B);
			DBG_C('C');DBG_X(cbuf);
			xmode = OWX_RUNNING;
			do_command(cbuf);
			DBG(0x1C);
			DBG_C('C');DBG_C('_');
			while(mode == OWM_WRITE)
				wait_complete('d');
			DBG(0x1D);
			set_idle();
		}
		else
			update_idle(2);
	} else if(bitp != 0x80) {
		DBG(0x1E);
		sei();
		uart_poll();
	} else {
		DBG(0x1F);
		sei();
	}

	if (mode == OWM_IDLE) {
		DBG(0x10);
		set_idle();
	}
	if (mode == OWM_SLEEP) {
		DBG(0x2F);
		return 0;
	}

	// RESET processing takes longer.
	update_idle((mode == OWM_SLEEP) ? 100
			: (mode <= OWM_PRESENCE) ? 20
			: (mode < OWM_IDLE || bitp < 0x80) ? 8
			: 1);

	return 1;
}
void onewire_poll(void) {
	DBG(0x3E);
	while(_onewire_poll()) ;
	DBG(0x2E);
}

// NOTE this disables interrupts!
void set_idle(void)
{
	/* This code will fail to recognize a reset if we're already in one.
	   Should happen rarely enough not to matter. */
	unsigned char sreg = SREG;
	cli();
#ifdef HAVE_UART // mode is volatile
	if(mode != OWM_SLEEP && mode != OWM_IDLE) {
#if 1
		DBG_C('R');
		DBG_N(mode);
#else
		DBG_P(">idle:");
		DBG_X(mode);
		DBG_C(' ');
		DBG_X(xmode);
		DBG_C(' ');
		DBG_X(bitp);
		DBG_NL();
#endif
	}
#endif

	DBG(0x30);
	mode = OWM_SLEEP;
	xmode = OWX_IDLE;
	wmode = OWW_NO_WRITE;
	CLEAR_LOW();
	DIS_TIMER();
	SET_FALLING();
	EN_OWINT();
	SREG = sreg;
}

TIMER_INT
{
	//Read input line state first
	//and copy a few globals to registers
	DBG_ON();DBG_OFF();DBG_ON();
	uint8_t p = !!(ONEWIRE_PIN&ONEWIRE_PBIT);
	mode_t lmode=mode;
	wmode_t lwmode=wmode;
	uint8_t lbitp=bitp;
	uint8_t lactbit=actbit;

	if (CHK_INT_EN()) {
		// reset pulse?
		if (p==0) { 
#if 0
			if(lmode != OWM_SLEEP) {
				DBG_P("\nReset ");
				DBG_X(lmode);
				DBG_NL();
			}
#endif
			lmode=OWM_IN_RESET;  //wait for rising edge
			lwmode=OWW_NO_WRITE;
			SET_RISING(); 
			CLEAR_LOW();
			//DBG_C('R');
		}
		DIS_TIMER();
	} else
	switch (lmode) {
	case OWM_IDLE:
		break;
	case OWM_SLEEP: // should have been caught by CHK_INT_EN() above
		DBG_P("\nChk Sleep!\n");
		break;
	case OWM_IN_RESET: // should not happen here
		DBG_P("\nChk Reset!\n");
		break;
	case OWM_AFTER_RESET:  //Time after reset is finished, now go to presence state
		lmode=OWM_PRESENCE;
		SET_LOW();
		SET_TIMER(OWT_PRESENCE);
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
			DBG(0x0F);
			DBG_P("\nRead OVR!\n");
			lmode = OWM_SLEEP;
		}
		break;
	case OWM_WRITE:
		CLEAR_LOW();
		if (lbitp) {
			lwmode = (cbuf & lbitp) ? OWW_WRITE_1 : OWW_WRITE_0;
			lbitp <<= 1;
		} else {
			lmode = OWM_IDLE;
			lwmode = OWW_NO_WRITE;
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
			DBG_C('x');
			DBG_N(bytep);
			DBG_X(lbitp);
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
		SET_TIMER(OWT_MIN_RESET-OWT_READLINE);  //OWT_READLINE around OWT_LOWTIME
		EN_OWINT();
	}
	mode=lmode;
	wmode=lwmode;
	bitp=lbitp;
	actbit=lactbit;
	DBG_OFF();
}

// 1wire level change.
// Do this in assembler so that writing a zero is _fast_.
void real_PIN_INT(void) __attribute__((signal));
void ONEWIREIRQ(void) __attribute__((naked));
void PIN_INT(void)
{
	// WARNING: No command in here may change the status register!
	DBG_ON();
	asm("     push r24");
	asm("     push r25");
	asm("     lds r24,wmode");
#ifdef DBGPORT
	asm("     out %0,r24" :: "i"(((int)&DBGPORT)-__SFR_OFFSET));
#endif
	asm("     ldi r25,%0" :: "i"(OWW_WRITE_0));
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
#warning "Ignore the 'appears to be a misspelled signal handler' warning"
void real_PIN_INT(void) {
	DIS_OWINT(); //disable interrupt, only in OWM_SLEEP mode it is active
#if 0 // def DBGPIN // modes are volatile
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
		uint8_t p = ONEWIRE_PIN & ONEWIRE_PBIT;
		DBG_C(p ? 'Q' : 'q');
	}
#endif
	//if (mode != OWM_SLEEP) DBG_C('a'+mode);
	wmode = OWW_NO_WRITE;

	switch (mode) {
	case OWM_PRESENCE:
		DBG_P("\nChk Presence!\n");
		break;
	case OWM_IDLE:
		DBG_P("\nChk Idle!\n");
		set_idle();
		/* fall thru */
	case OWM_SLEEP:
		SET_TIMER(OWT_MIN_RESET);
		EN_OWINT(); //any earlier edges will simply reset the timer
		break;
	//start of reading with falling edge from master, reading closed in timer isr
	case OWM_READ:
	case OWM_SEARCH_READ:   //Search algorithm waiting for receive or send
		SET_TIMER(OWT_READLINE); //wait a time for reading
		break;
	case OWM_SEARCH_ZERO:   //Search algorithm waiting for receive or send
	case OWM_SEARCH_ONE:   //Search algorithm waiting for receive or send
	case OWM_WRITE: //a bit is sending 
		SET_TIMER(OWT_LOWTIME);
		break;
	case OWM_IN_RESET:  //rising edge of reset pulse
		SET_TIMER(OWT_RESET_PRESENCE);  //wait before sending presence pulse
		mode=OWM_AFTER_RESET;
		SET_FALLING();
		//DBG_C('r');
		break;
	case OWM_AFTER_RESET: // some other chip was faster, assert my own presence signal now
		SET_TIMER(0);
		break;
	}
	EN_TIMER();
	DBG_OFF();
//	if (mode > OWM_PRESENCE)
//		DBG_T(1);
}

