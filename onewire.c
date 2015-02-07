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
#define T_(c) ((F_CPU/64)/(1000000/c))
#define OWT_MIN_RESET T_(410)
#define OWT_RESET_PRESENCE (T_(40)-1)
#define OWT_PRESENCE T_(160)
#define OWT_READLINE (T_(35)-1)
#define OWT_LOWTIME (T_(40)-2)

#if (OWT_MIN_RESET>240)
#error Reset timing is broken, your clock is too fast
#endif
#if (OWT_READLINE<2)
#error Read timing is broken, your clock is too slow
#endif

#define EN_OWINT() do {IMSK|=(1<<INT0);IFR|=(1<<INTF0);}while(0)  //enable interrupt 
#define DIS_OWINT() do {IMSK&=~(1<<INT0);} while(0)  //disable interrupt
#define SET_RISING() do {MCUCR|=(1<<ISC01)|(1<<ISC00);}while(0)  //set interrupt at rising edge
#define SET_FALLING() do {MCUCR|=(1<<ISC01);MCUCR&=~(1<<ISC00);} while(0) //set interrupt at falling edge
#define CHK_INT_EN (IMSK&(1<<INT0))==(1<<INT0) //test if interrupt enabled
#define PIN_INT INT0_vect  // the interrupt service routine
//Timer Interrupt
#define EN_TIMER() do {TIMSK |= (1<<TOIE0); TIFR|=(1<<TOV0);}while(0) //enable timer interrupt
#define DIS_TIMER() do {TIMSK &= ~(1<<TOIE0);} while(0) // disable timer interrupt

// always use timer 0
#define TCNT_REG TCNT0  //register of timer-counter
#define TIMER_INT ISR(TIMER0_OVF_vect) //the timer interrupt service routine

#define BAUDRATE 57600

// stupidity
#ifndef TIMER0_OVF_vect
#  define TIMER0_OVF_vect TIM0_OVF_vect
#endif

#define SET_LOW() do { OWDDR|=(1<<ONEWIREPIN);} while(0)  //set 1-Wire line to low
#define CLEAR_LOW() do {OWDDR&=~(1<<ONEWIREPIN);} while(0) //set 1-Wire pin as input

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

static inline void set_timer(int timeout)
{
	//DBG_C('T');
	//DBG_X(timeout);
	//DBG_C(',');
	TCNT0 = ~timeout;
	TIFR0 |= (1 << TOV0);
	TIMSK |= (1 << TOIE0);
}


//States / Modes
typedef enum {
	OWM_SLEEP,  //Waiting for next reset pulse
	OWM_CHK_RESET,  //waiting of rising edge from reset pulse
	OWM_RESET,  //Reset pulse received 
	OWM_PRESENCE,  //sending presence pulse
	OWM_READ, //read some bits
	OWM_WRITE, //write some bits
	OWM_IDLE, //waiting for non-IRQ space
	OWM_SEARCH_ZERO,  //SEARCH_ROM algorithm
	OWM_SEARCH_ONE,
	OWM_SEARCH_READ,
} mode_t;
volatile mode_t mode; //state

//next high-level state
typedef enum {
	OWX_IDLE, // nothing is happening
	OWX_COMMAND, // reading a command
	OWX_READY, // waiting for non-interrupt space
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
volatile uint8_t actbit; // current bit
volatile uint8_t srcount; // counter for search rom

static inline void clear_timer(void)
{
	//DBG_C('t');
	//TCNT0 = 0;
	TIMSK &= ~(1 << TOIE0);	   // turn off the timer IRQ
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
	while(state & (S_RECV|S_XMIT)) {
#ifdef HAVE_UART
		uart_poll();
#endif
		update_idle(vbitcount);
	}
	cli();
	if(!S_IN_APP(state)) {
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
	bitcount = len;
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
	while(state & (S_RECV|S_XMIT)) {
#ifdef HAVE_UART
		uart_poll();
#endif
		update_idle(vbitcount);
	}
	cli();
	if(!S_IN_APP(state)) {
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

static uint8_t recv_any_in(void)
{
	while(state & S_RECV) {
#ifdef HAVE_UART
		uart_poll();
#endif
		update_idle(vbitcount);
	}
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

	state = S_IDLE;
	setup();
	DBG_T(1);
	DBG_T(1);

	set_idle();
	DBG_T(4);

	// now go
	sei();
	DBG_ON();
	DBG_P("\nInit done!\n");

	setjmp(end_out);
	DBG_OFF();
	while (1) {
#ifdef HAVE_UART
		volatile unsigned long long int x; // for really bad timing
		DBG_C('/');
		for(x=0;x<100000ULL;x++)
#endif
		 {
#ifdef HAVE_UART
			uart_poll();
#endif
			if(state == S_HAS_OPCODE)
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
	cli();
	if(mode != OWM_SLEEP) {
		DBG_P(">idle:");
		DBG_X(mode);
		DBG_P(" b");
		DBG_N(xmitlen);
		DBG_N(bitcount);
		DBG_NL();
		mode = OWM_SLEEP;
	}
	DBG_OFF();
	//DBG_OUT();

	bitcount = 0;
	xmitlen = 0;

	mode = OWM_SLEEP;
	wmode = OWW_NO_WRITE;
	DIS_TIMER();
	SET_FALLING();
	EN_OWINT();

	IFR |= (1 << INTF0);		// ack+enable level-change interrupt, just to be safe
	IMSK |= (1 << INT0);
	OWDDR &= ~(1 << ONEWIREPIN);	// set to input
	sei();
}

#if 0
static void set_reset(void) {
	state = S_RESET;
	bitcount = 8;
	xmitlen = 0;
	DBG_C('\n');
	DBG_C('R');
	//DBG_OUT();
}
#endif

static inline void start_reading(uint8_t bits) {
			mode = OWM_READ;
			cbuf=0;
			bitp=1<<(8-bits);
}
#define START_READING(bits) do { \
			lmode = OWM_READ; \
			cbuf=0; \
			lbitp=1<<(8-bits); \
} while(0)

TIMER_INT {
	wmode_t lwmode=wmode; //let these variables be in registers
	mode_t lmode=mode;
	uint8_t lbytep=bytep;
	uint8_t lbitp=bitp;
	uint8_t lactbit=actbit;

	//Read input line state 
	uint8_t p=!!(OWPIN&(1<<ONEWIREPIN));

	switch (lmode) {
	case OWM_IDLE:
		DBG_P("\nChk Idle!\n");
		break;
	case OWM_SLEEP:
		if (p==0) { 
			lmode=OWM_CHK_RESET;  //wait for rising edge
			SET_RISING(); 
		}
		DIS_TIMER();
		break;
	case OWM_CHK_RESET: // should not happen here
		DBG_P("\nChk Reset!\n");
		break;
	case OWM_RESET:  //Reset pulse and time after is finished, now go in presence state
		lmode=OWM_PRESENCE;
		SET_LOW();
		TCNT_REG=~(OWT_PRESENCE);
		DIS_OWINT();  //No Pin interrupt necessary only wait for presence is done
		break;
	case OWM_PRESENCE:
		CLEAR_LOW();  //Presence is done now wait for a command
		START_READING(8);
		xmode = OWX_COMMAND;
		break;
	case OWM_READ:
		if(lbitp) {
			if (p)  // Set bit if line high 
				cbuf |= lbitp;
			lbitp <<= 1;
		} else {
			// Overrun!
			DBG_P("\nRead OVR!\n");
			set_idle();
			return;
		}
		break;
	case OWM_WRITE:
		if (lbitp) {
			lwmode = (cbuf & lbitp) ? OWW_WRITE_1 : OWW_WRITE_0;
			lbitp <<= 1;
		} else {
			// Overrun!
			DBG_P("\nWrite OVR!\n");
			set_idle();
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
		if (p != lactbit) {  //check master bit
			lmode=OWM_SLEEP;  //not the same: go sleep
			break;
		}

		lbitp=(lbitp<<1);  //prepare next bit
		if (!lbitp) {
			lbitp=1;
			lbytep++;
			if (lbytep>=8) {
				lmode=OWM_SLEEP;  //all bits processed 
				break;
			}
		}				
		lmode = OWM_SEARCH_ZERO;
		lactbit = !!(ow_addr[lbytep]&lbitp);
		lwmode = lactbit ? OWW_WRITE_1 : OWW_WRITE_0;
		break;
	}
	if (lmode == OWM_SLEEP)
		DIS_TIMER();
	if (lmode != OWM_PRESENCE)  { 
		TCNT_REG=~(OWT_MIN_RESET-OWT_READLINE);  //OWT_READLINE around OWT_LOWTIME
		EN_OWINT();
	}
	mode=lmode;
	wmode=lwmode;
	bytep=lbytep;
	bitp=lbitp;
	actbit=lactbit;
}

#if 0 // OLD
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
		set_timer(OWT_PRESENCE);
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
				transbyte = ow_addr[--xmitlen];
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
			transbyte = ow_addr[7];
		}
		else
#ifndef SKIP_SEARCH
		if (transbyte == 0xF0) {
			state = S_SEARCHROM;
			xmitlen = 7;
			transbyte = ow_addr[7];
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
			transbyte = ow_addr[--xmitlen];
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
		if (transbyte != ow_addr[--xmitlen]) {
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
#endif // OLD


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

void real_PIN_INT(void) {
	DIS_OWINT(); //disable interrupt, only in OWM_SLEEP mode it is active
	wmode = OWW_NO_WRITE;
	switch (mode) {
		case OWM_PRESENCE:
			DBG_P("\nChk Presence!\n");
			break;
		case OWM_IDLE:
			DBG_P("\nChk Idle!\n");
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
		case OWM_CHK_RESET:  //rising edge of reset pulse
			SET_FALLING();
			TCNT_REG=~(OWT_RESET_PRESENCE);  //wait before sending presence pulse
			mode=OWM_RESET;
			break;
		case OWM_RESET: // some other chip was faster, asserting presence
			start_reading(8);
			xmode = OWX_COMMAND;
			break;
	}
	EN_TIMER();
}

#if 0 // OLD
void real_PIN_INT(void) {
	DBG_T(1);


	uint8_t st = state & S_MASK;
	if (OWPIN & (1 << ONEWIREPIN)) {	 // low => high transition
		DBG_TS();
		//DBG_C('^');
#ifdef HAVE_TIMESTAMP
		TCCR1B &=~ (1<<ICES1);
#endif
		if (((TCNT0 < 0xF0)||(st == S_IDLE)) && (TCNT0 > T_RESET)) {	// Reset pulse seen
			set_timer(OWT_RESET_PRESENCE);
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
			set_timer(OWT_LOWTIME);
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
				transbyte = ow_addr[--xmitlen];
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
		set_timer(OWT_READLINE);
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
#endif // OLD


