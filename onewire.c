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

#ifdef HAVE_EEPROM
static uint8_t ow_addr[8];
#else
static const uint8_t ow_addr[8]={0x1D, 0x19, 0x00, 0x00, 0x00, 0x00, 0xc5, 0xFB};
#endif

volatile uint8_t bitp;  // mask of current bit
volatile uint8_t bytep; // position of current byte
volatile uint8_t cbuf;  // char buffer, current byte to be (dis)assembled

static jmp_buf end_out;


#ifdef __AVR_ATtiny13__
static inline void mcu_init(void) {
	CLKPR = 0x80;	 // Prepare to ...
	CLKPR = 0x00;	 // ... set to 9.6 MHz

	TCCR0A = 0;
	TCCR0B = 0x03;	// Prescaler 1/64

	MCUCR |= (1 << ISC00);		  // Interrupt on both level changes
}

#elif defined(__AVR_ATtiny25__)

static inline void mcu_init(void) {
	CLKPR = 0x80;	 // Prepare to ...
	CLKPR = 0x00;	 // ... set to 8.0 MHz

	MCUCR |= (1 << ISC00);		  // Interrupt on both level changes
}

#elif defined(__AVR_ATtiny84__)
static inline void mcu_init(void) {
	CLKPR = 0x80;	 // Prepare to ...
	CLKPR = 0x00;	 // ... set to 8.0 MHz

	MCUCR |= (1 << ISC00);		  // Interrupt on both level changes
}

#elif defined (__AVR_ATmega8__)
static inline void mcu_init(void) {
	// Clock is set via fuse
	// CKSEL = 0100;   Fuse Low Byte Bits 3:0

	TCCR0 = 0x03;	// Prescaler 1/64

	MCUCR |= (1 << ISC00);		  // Interrupt on both level changes
}

#elif defined (__AVR_ATmega168__) || defined (__AVR_ATmega88__)
static inline void mcu_init(void) {

	// Clock is set via fuse

	TCCR0A = 0;
	TCCR0B = 0x03;	// Prescaler 1/64

	EICRA = (1<<ISC00); // interrupt of INT0 (pin D2) on both level changes
}

#else
#error Pinout for your CPU undefined
#endif

#ifndef TIMSK
#define TIMSK TIMSK0
#endif
#ifndef TIFR
#define TIFR TIFR0
#endif

// Frequency-dependent timing macros
#define T_(c) ((F_CPU/PRESCALE)/(1000000/c))
#define OWT_MIN_RESET T_(410)
#define OWT_RESET_PRESENCE (T_(40)-1)
#define OWT_PRESENCE T_(160)
#define OWT_READLINE (T_(35)-2)
#define OWT_LOWTIME (T_(40)-2)

#if (OWT_MIN_RESET>240)
#error Reset timing is broken, your clock is too fast
#endif
#if (OWT_READLINE<2)
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

#define BAUDRATE 38400

// stupidity
#ifndef TIMER0_OVF_vect
#  define TIMER0_OVF_vect TIM0_OVF_vect
#endif

#define SET_LOW() do { OWDDR|=(1<<ONEWIREPIN);} while(0)  //set 1-Wire line to low
#define CLEAR_LOW() do {OWDDR&=~(1<<ONEWIREPIN);} while(0) //set 1-Wire pin as input

static void do_select(uint8_t cmd);

// Initialise the hardware
void setup(void)
{

	mcu_init();

	OWPORT &= ~(1 << ONEWIREPIN);
	OWDDR &= ~(1 << ONEWIREPIN);

#ifdef HAVE_TIMESTAMP
	TCCR1A = 0;
	TCCR1B = (1<<ICES1) | (1<<CS10);
	TIMSK1 &= ~(1<<ICIE1);
	TCNT1 = 0;
#endif

#ifdef HAVE_EEPROM
	// Get 64bit address from EEPROM
	{
		unsigned char i;
		while(EECR & (1<<EEPE));	 // Wait for EPROM circuitry to be ready
		for (i=8; i;) {
			i--;
			/* Set up address register */
			EEARL = 7-i;			   // set EPROM Address
			/* Start eeprom read by writing EERE */
			EECR |= (1<<EERE);
			/* Return data from data register */
			ow_addr[i] =  EEDR;
		}
	}
#endif

	// init application-specific code
	init_state();

	IFR |= (1 << INTF0);
	IMSK |= (1 << INT0);

#ifdef HAVE_UART
	uart_init(UART_BAUD_SELECT(BAUDRATE,F_CPU));
#endif
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
	OWX_SELECT, // reading a selector (does the master even talk to us?)
	OWX_COMMAND, // reading a command
	OWX_RUNNING, // waiting for non-interrupt space
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
	sei();
	DBGS_P(".e1");
	longjmp(end_out,1);
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

static inline void wait_complete(char c)
{
	if(bitp || (wmode != OWW_NO_WRITE))
		DBG_C(c);
	while(1) {
		if (mode < OWM_IDLE) {
			DBGS_P("s5");
			longjmp(end_out,1);
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
	DBGS_P(".e4");

	xmode = OWX_COMMAND;
	longjmp(end_out,1);
}

static void
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
	DBG_OFF();
}

void xmit_bit(uint8_t val)
{
	xmit_any(!!val,1);
}

void xmit_byte(uint8_t val)
{
	xmit_any(val,8);
}

uint8_t rx_ready(void)
{
	if (mode <= OWM_IDLE)
		return 1;
	return !bitp;
}

static void
recv_any(uint8_t len)
{
	wait_complete('j');
	cli();
	if(mode == OWM_WRITE || mode == OWM_IDLE)
		mode = OWM_READ;
	if (mode != OWM_READ || xmode <= OWX_SELECT) {
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

static uint8_t recv_any_in(void)
{
	wait_complete('i');
	if (mode != OWM_READ) {
		DBGS_P(".e2");
		longjmp(end_out,1);
	}
	mode = OWM_IDLE;
	return cbuf;
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

	setjmp(end_out);
	while (1) {
#ifdef HAVE_UART
		volatile unsigned long long int x; // for really bad timing
		DBGS_C('/');
		for(x=0;x<100000ULL;x++)
#endif
		 {
#ifdef HAVE_UART
			uart_poll();
#endif
			DBG_OFF();
			if(xmode == OWX_SELECT && !bitp) {
				//DBG_C('S');
				//DBG_X(cbuf);
				xmode = OWX_RUNNING;
				do_select(cbuf);
			}
			if(xmode == OWX_COMMAND && !bitp) {
				DBG_C('C');
				DBG_X(cbuf);
				xmode = OWX_RUNNING;
				do_command(cbuf);
			}

			// RESET processing takes longer.
			update_idle((mode == OWM_SLEEP) ? 100 : (mode <= OWM_AFTER_RESET) ? 20 : (mode < OWM_IDLE) ? 8 : 1); // TODO

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
	cli();
	if(mode != OWM_SLEEP) {
		DBGS_P(">idle:");
		DBGS_X(mode);
		DBGS_C(' ');
		DBGS_X(xmode);
		DBGS_C(' ');
		DBGS_X(bitp);
		DBGS_NL();
	}
	DBG_OFF();
	//DBG_OUT();

	mode = OWM_SLEEP;
	xmode = OWX_IDLE;
	wmode = OWW_NO_WRITE;
	CLEAR_LOW();
	DIS_TIMER();
	SET_FALLING();
	DBG_C('s');
	EN_OWINT();

	IFR |= (1 << INTF0);		// ack+enable level-change interrupt, just to be safe
	IMSK |= (1 << INT0);
	OWDDR &= ~(1 << ONEWIREPIN);	// set to input
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
		cbuf = ow_addr[0];
		actbit = cbuf&1;
		wmode = actbit ? OWW_WRITE_1 : OWW_WRITE_0;
		return;
	case 0x55: // MATCH_ROM
		recv_byte();
		for (i=0;;i++) {
			uint8_t b = recv_byte_in();
			if (b != ow_addr[i]) {
				DBGS_P("\nMR ");
				DBGS_X(i);
				DBGS_C('=');
				DBGS_X(b);
				next_idle();
			}
			if (i < 7)
				recv_byte();
			else
				break;
		}
		next_command();
#ifdef _ONE_DEVICE_CMDS_
	case 0xCC: // direct access
		next_command();
	case 0x33:
		for (i=0;i<8;i++) {
			xmit_byte(ow_addr[i]);
		
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
		DBG_ON();
	wmode_t lwmode=OWW_NO_WRITE; // wmode; //let these variables be in registers
	uint8_t lbytep=bytep;
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
			set_idle();
			DBG_OFF();
			return;
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
			set_idle();
			DBG_OFF();
			return;
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
			lbytep++;
			if (lbytep>=8) {
				START_READING(8);
				//DBG_P("S2");
				xmode = OWX_COMMAND;
				break;
			}
			lbitp=1;
			cbuf = ow_addr[lbytep];
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
	bytep=lbytep;
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
	if (mode > OWM_PRESENCE)
		DBG_ON();
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
	DBG_OFF();
//	if (mode > OWM_PRESENCE)
//		DBG_T(1);
}

