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

#define MAIN
#include "features.h"
#include "onewire.h"
#include "uart.h"
#include "dev_data.h"
#include "debug.h"

// Initialise the hardware
static inline void
setup(void)
{
	mcu_init();
	uart_init(UART_BAUD_SELECT(BAUDRATE,F_CPU));
	onewire_init();
}

// Main program
int
main(void)
{
#ifdef DBGPIN
	OWPORT &= ~(1 << DBGPIN);
	OWDDR |= (1 << DBGPIN);
#endif
	OWDDR &= ~(1<<ONEWIREPIN);
	OWPORT &= ~(1<<ONEWIREPIN);

	DBG_IN();

#ifdef HAVE_TIMESTAMP
	tbpos = sizeof(tsbuf)/sizeof(tsbuf[0]);
	uint16_t last_tb = 0;
#endif

	setup();

	set_idle();

	// now go
	sei();
	DBGS_P("\nInit done!\n");
	while(1) mainloop();
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

