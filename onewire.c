
#include <avr/io.h>
#include <avr/interrupt.h>

#ifdef __AVR_ATtiny13__
#define F_CPU				 9600000
#endif
#ifdef __AVR_ATmega168__
#define F_CPU				 16000000
#endif

#ifndef F_CPU
#error Need to define F_CPU
#endif

#undef  EMU_18b20 // temperature
#define EMU_2423  // counter

// Prototypes
void ioinit(void);
void set_idle(void);

// 1wire interface
unsigned char status, cmd, bitcount, transbyte, bytecount ;
unsigned char shift_reg, fb_bit ;
unsigned char addr[8] = "\x1d\x12\x23\x34\x45\x56\x67\x39"; // includes family code and CRC

unsigned char transdata[9];   // store sent/received bytes, 18b20 compatible

// Basic bus state machine: variable "status"
//  Bitmasks
#define S_RECV 0x01
#define S_XMIT 0x02
#define S_MASK 0x7F
#define S_XMIT2 0x80 // flag to de-assert zero bit on xmit timeout

//  initial states
#define S_IDLE            (       0x00) // wait for Reset
#define S_RESET           (       0x04) // Reset seen
#define S_PRESENCEPULSE   (       0x08) // sending Presence pulse
#define S_RECEIVE_ROMCODE (S_RECV|0x04) // selection opcode
#define S_RECEIVE_OPCODE  (S_RECV|0x08) // real opcode
//  selection opcode states
#define S_MATCHROM        (S_RECV|0x10) // select a known slave
#define S_READROM         (S_XMIT|0x10) // single slave only!
#define S_SEARCHROM       (S_XMIT|0x14) // search, step 1: send ID bit
#define S_SEARCHROM_I     (S_XMIT|0x18) // search, step 2: send inverted ID bit
#define S_SEARCHROM_R     (S_RECV|0x14) // search, step 3: check what the master wants
//  real opcode states
#define S_CMD_RECV        (S_RECV|0x20) // receive a byte
#define S_CMD_XMIT        (S_XMIT|0x20) // send a byte

// command state machine: variable "cmd"
#ifdef EMU_2423
#define C_WRITE_SCRATCHPAD 0x0F // TODO
#define C_READ_SCRATCHPAD  0xAA // TODO
#define C_COPY_SCRATCHPAD  0x55 // TODO
#define C_READ_MEM         0xF0 // TODO
#define C_READ_MEM_COUNTER 0xA5 // TODO
#endif

#ifdef EMU_18b20
#define S_READMEM       0xBE
//#define S_WRITEEEPROM   ?? // TODO
#define S_WRITEMEM      0x4E
#endif


// 1wire ID
#define ADIN0 0x02
#define ADIN1 0x03

#ifdef __AVR_ATtiny13__
#define OWPIN PINB
#define OWPORT PORTB
#define OWDDR DDRB
#define ONEWIREPIN 1		 // INT0

#define IMSK GIMSK

#elif defined (__AVR_ATmega8__)
#define OWPIN PIND
#define OWPORT PORTD
#define OWDDR DDRD
#define ONEWIREPIN 2		// INT0

#define IMSK GIMSK
#define TIMSK0 TIMSK
#define TIFR0 TIFR
#define EEPE EEWE
#define EEMPE EEMWE

#define HAVE_UART

#elif defined (__AVR_ATmega168__)
#define OWPIN PIND
#define OWPORT PORTD
#define OWDDR DDRD
#define ONEWIREPIN 2		// INT0
#define DBGPIN 3		// debug output

#define IMSK EIMSK

#undef HAVE_UART
#undef HAVE_TIMESTAMP

#endif

#define T_PRESENCE 15
#define T_PRESENCEWAIT 2
#define T_SAMPLE 3		   // War 5 jetzt 3
#define T_XMIT 5		   // War 15 jetzt auf 10
#define T_RESET_ 50        // 
#define T_RESET (T_RESET_-T_SAMPLE)
#define BAUDRATE 57600

#ifdef HAVE_UART
#include "uart.h"

#define DBG_C(x) uart_putc(x)
#define DBG_P(x) uart_puts_P(x)
#define DBG_N(x) uart_puthex_nibble(x)
#define DBG_X(x) uart_puthex_byte(x)
#define DBG_Y(x) uart_puthex_word(x)
#define DBG_NL() uart_putc('\n')
#ifdef HAVE_TIMESTAMP
volatile unsigned char tbpos;
volatile uint16_t tsbuf[100];
#define DBG_TS(void) do { if(tbpos) tsbuf[--tbpos]=ICR1; } while(0)
#endif
#else
#define DBG_C(x) do { } while(0)
#define DBG_P(x) do { } while(0)
#define DBG_X(x) do { } while(0)
#define DBG_N(x) do { } while(0)
#define DBG_NL() do { } while(0)
#endif
#ifndef DBG_TS
#define DBG_TS() do { } while(0)
#endif

#ifdef DBGPIN
#define DBG_ON() OWPORT |= (1<<DBGPIN)
#define DBG_OFF() OWPORT &= ~(1<<DBGPIN)
#else
#define DBG_ON() do { } while(0)
#define DBG_OFF() do { } while(0)
#endif

// stupidity
#ifndef TIMER0_OVF_vect
#  define TIMER0_OVF_vect TIM0_OVF_vect
#endif


// Initialisierung der Hardware
void ioinit(void)
{
	unsigned char i;
#ifdef __AVR_ATtiny13__
	CLKPR = 0x80;	 // Per software auf 9,6 MHz stellen Aenderung des Vorteilers freigeben
	CLKPR = 0x00;	 // Vorteiler auf 1 (000) setzen

	TCCR0A = 0;
	TCCR0B = 0x03;	// Prescaler 1/64

	MCUCR |= (1 << ISC00);		  // Interrupt auf beide Flanken

#elif defined (__AVR_ATmega8__)
	// Clock is set via fuse
	// CKSEL = 0100;   Fuse Low Byte Bits 3:0

	TCCR0 = 0x03;	// Prescaler 1/64

	MCUCR |= (1 << ISC00);		  // Interrupt auf beide Flanken

#elif defined (__AVR_ATmega168__)
	// Clock is set via fuse

	TCCR0A = 0;
	TCCR0B = 0x03;	// Prescaler 1/64

	EICRA = (1<<ISC00); // interrupt of INT0 (pin D2) on both level changes

#else
#error Not yet implemented
#endif

#if 0
	ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1);
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

	EIFR |= (1 << INTF0);
	IMSK |= (1 << INT0);

	// Get data from EEPROM
	while(EECR & (1<<EEPE));	 // Wait for previous write to finish
	for (i=2; i<4; i++) {
		/* Set up address register */
		EEARL = i;			   // set EPROM Address
		/* Start eeprom read by writing EERE */
		EECR |= (1<<EERE);
		/* Return data from data register */
		transdata[i] =  EEDR;
	}

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

// Das Hauptprogramm (Einsprungpunkt)
int main(void)
{
#if 0
	unsigned char i, dummy;
	unsigned char hyst;
#endif
#ifdef HAVE_TIMESTAMP
	tbpos = sizeof(tsbuf)/sizeof(tsbuf[0]);
	uint16_t last_tb = 0;
#endif

	status = S_IDLE;
	ioinit();
	set_idle();
	unsigned char lev = 0;

	// now go
	sei();
	DBG_P("\nInit done!\n");

	while (1) {
		volatile unsigned long long int x;
		DBG_C('/');
		for(x=0;x<100000ULL;x++) {
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
#if 0
		// first input
		ADMUX = ADIN0;
		//	ADMUX |= (1<<ADLAR);	// Externe Referenz, Obere 8 Bit alignen (untere zwei Bit wegschmeissen)

		// dummy read
		ADCSRA |= (1<<ADSC);
		while(ADCSRA & (1<<ADSC));

		// real read
		ADCSRA |= (1<<ADSC);
		while(ADCSRA & (1<<ADSC));

		transdata[0] = ADCL;
		dummy = ADCH;

		// second input
		ADMUX = ADIN1;

		// dummy read
		ADCSRA |= (1<<ADSC);
		while(ADCSRA & (1<<ADSC));

		// real read
		ADCSRA |= (1<<ADSC);
		// Auf Ergebnis warten...
		while(ADCSRA & (1<<ADSC));

		transdata[1] = ADCL;
		dummy = ADCH;

		if (transdata[0] > transdata[1] + transdata[2] - hyst) {
			hyst = transdata[3];
		 } else {   
			hyst = 0;
		 }
		 transdata[4] = hyst;		
#endif
	}
}

void set_idle(void)
{
	if(status != S_IDLE) {
		DBG_P(">idle:");
		DBG_X(status);
		DBG_P(" b");
		DBG_N(bytecount);
		DBG_N(bitcount);
		DBG_NL();
		status = S_IDLE;
	}

	bitcount = 0;
	bytecount = 0;

	clear_timer();
	EIFR |= (1 << INTF0);		// ack+enable level-change interrupt, just to be safe
	IMSK |= (1 << INT0);
}

void set_reset() {
	status = S_RESET;
	bitcount = 0;
	bytecount = 0;
	DBG_C('\n');
	DBG_C('R');
}


// Timer interrupt routine
ISR (TIMER0_OVF_vect)
{
	unsigned char tim0_i, pin, st = status & S_MASK;
	pin = OWPIN & (1 << ONEWIREPIN);
	clear_timer();
	//DBG_C(pin ? '!' : ':');
	if (status & S_XMIT2) {
		status &= ~S_XMIT2;
		EIFR |= (1 << INTF0);
		IMSK |= (1 << INT0);
		OWDDR &= ~(1 << ONEWIREPIN);	// set to input
		//DBG_C('x');
	}
	else if (st == S_RESET) {       // send a presence pulse
		OWDDR |= (1 << ONEWIREPIN);
		status = S_PRESENCEPULSE;
		set_timer(T_PRESENCE);
		DBG_C('P');
	}
	else if (st == S_PRESENCEPULSE) {
		OWDDR &= ~(1 << ONEWIREPIN);	// Presence pulse done
		status = S_RECEIVE_ROMCODE;		// wait for command
		DBG_C('O');
	}
	else if (st & S_RECV) {
		if (st == S_SEARCHROM_R) {
			DBG_ON();
			//DBG_C((pin != 0) + '0');
			if (((transbyte & 0x01) == 0) != (pin == 0)) {
				DBG_C('-');
				//DBG_X(transbyte);
				set_idle();
				DBG_OFF();
			}
			else {
				DBG_OFF();
				//DBG_C(' ');
				transbyte >>= 1;
				status = st = S_SEARCHROM;
				bitcount++;
			}
		}
		else {
			DBG_ON();
			if (!bitcount)
				transbyte = 0;
			else
				transbyte >>= 1;
			if (pin)
				transbyte |= 0x80;
			bitcount++;
		}
		if (bitcount == 8) {
			bitcount = 0;
			if (st == S_RECEIVE_ROMCODE) {
				bytecount = 0;
				shift_reg = 0;
				DBG_X(transbyte);
				DBG_C(':');
				if (transbyte == 0x55) {
					status = S_MATCHROM;
				}
				else if (transbyte == 0xCC) {
					 // skip ROM; nothing to do, just wait for next command
					 status = S_RECEIVE_OPCODE;
					 DBG_C('C');
				}
				else if (transbyte == 0x33) {
					status = S_READROM;
					transbyte = addr[0];
				}
				else if (transbyte == 0xF0) {
					status = S_SEARCHROM;
					transbyte = addr[0];
					DBG_C('?');
					DBG_X(transbyte);
				}
				else {
					DBG_P("::Unknown ");
					DBG_X(transbyte);
					set_idle();
				}
			}
			else if (st == S_RECEIVE_OPCODE) {
				bytecount = 0;
				shift_reg = 0;
				DBG_X(transbyte);
				DBG_C(':');
#if 0
				if (transbyte == 0x4E) {
					status = S_CMD_READ;
					cmd = C_WRITEMEM;
				}
				else if (transbyte == 0x48) {  // Write data to EEPROM
					for (tim0_i = 2; tim0_i < 4; tim0_i++) {
						while(EECR & (1<<EEPE)) ;
						/* Set up address and data registers */
						EEARL = tim0_i;			// EEPROM Address
						EEDR = transdata[tim0_i];
						/* Write logical one to EEMPE */
						EECR |= (1<<EEMPE);
						/* Start eeprom write by setting EEPE */
						EECR |= (1<<EEPE);
					}
					set_idle();
				}
				else if (transbyte == 0xBE) {
					status = S_READMEM;
					cmd = C_READMEM;
					transbyte = transdata[0];
				}
				else
#endif
				 {
					DBG_P(":Unknown ");
					DBG_X(transbyte);
					set_idle();
				}
			}
			else if (st == S_SEARCHROM) {
				if (bytecount < 7) {
					bytecount++;
					transbyte = addr[bytecount];
					DBG_C('?');
					DBG_X(transbyte);
				}
				else {
					status = S_RECEIVE_OPCODE;
					DBG_C('C');
				}
			}
			else if (st == S_MATCHROM) {
				if (transbyte != addr[bytecount])
					set_idle();
				else if (bytecount < 7)
					bytecount++;
				else {
					status = S_RECEIVE_OPCODE;
					DBG_C('C');
				}
			}
#if 0
			else if (st == S_WRITEMEM) {
				if (bytecount == 0) {
					transdata[2] = transbyte;
				} 
				else {
					transdata[3] = transbyte;
					status = S_RECEIVE_OPCODE;
					DBG_C('C');
				}
			}		
#endif
			else
				set_idle();
		}
		DBG_OFF();
	}
	else {
		OWDDR &= ~(1 << ONEWIREPIN);	// set to input
	}
	//DBG_C('.');
	DBG_OFF();
}


// 1wire level change
ISR (INT0_vect)
{
	unsigned char tim0_i, st = status & S_MASK;
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
	}
	else {									  // high => low transition
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
				status |= S_XMIT2;
			} else
				clear_timer();
			if (st == S_SEARCHROM) {
				//DBG_C((transbyte & 0x01) + '0');
				transbyte ^= 0x01;
				status = (status & S_XMIT2) | S_SEARCHROM_I;
			} else if (st == S_SEARCHROM_I) {
				status = (status & S_XMIT2) | S_SEARCHROM_R;
				//DBG_C((transbyte & 0x01) + '0');
				transbyte ^= 0x01;
			} else {
				//DBG_C((transbyte & 0x01) ? 'H' : 'L');
#if 0
				if (st == S_READMEM) { // calculate CRC8
					fb_bit = (transbyte ^ shift_reg) & 0x01;
					shift_reg >>= 1;
					if (fb_bit)
						shift_reg ^= 0x8c;
				}
#endif
				bitcount++;
				transbyte >>= 1;
				if (bitcount == 8) {
					bitcount = 0;
					bytecount++;
#if 0
					if (st == S_READMEM) {
						if (bytecount == 8)
							 transbyte = shift_reg;  // CRC senden
						else if (bytecount == 9)
							status = S_IDLE;  // CRC senden
						else
							transbyte = transdata[bytecount];
					}
					else
#endif
					if (st == S_READROM) {
						if (bytecount == 8) {
							status = S_RECEIVE_OPCODE;
							DBG_C('C');
						}
						else
							transbyte = addr[bytecount];
					}
				}
			}
		}
		else if (st == S_IDLE) {		   // first 1-0 transition
			clear_timer();
		}
		else if (status & S_RECV) {
			set_timer(T_SAMPLE);
		}
		else if (st == S_RESET) {		   // somebody else sends their Presence pulse
			status = S_RECEIVE_ROMCODE;
			clear_timer();
		}
		else if (st == S_PRESENCEPULSE)
			;   // do nothing, this is our own presence pulse
		else {
			set_idle();
		}		
	}
}
