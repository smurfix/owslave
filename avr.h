/*
 * avr.h
 *  only included if compiling for AVR cpu
 */

#ifndef AVR_H_
#define AVR_H_

// probably useless #include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
// debugging only
#include "uart.h"

// AVRs use 8 bit timers
typedef unsigned char timer_t;

/* hardware specific settings, just as the cpu should come from
 * either the makefile or better something like Kconfig
 */
#ifndef F_CPU
#warning "CPU frequency not defined, assuming 8MHz"
#define F_CPU	8000000
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


#define OW_PINCHANGE_ISR() ISR (INT0_vect)
#define OW_TIMER_ISR() ISR (TIMER0_OVF_vect)

// stupidity
#ifndef TIMER0_OVF_vect
#  define TIMER0_OVF_vect TIM0_OVF_vect
#endif

// for atmega8 and atmega32
#ifndef EEPE
#define EEPE EEWE
#endif
#ifndef EEMPE
#define EEMPE EEMWE
#endif

// this works for all AVRs, getting address from address 0..7
static inline void get_ow_address(u_char *addr)
{
	u_char i;

	 // Wait for EPROM circuitry to be ready
	while(EECR & (1<<EEPE));

	for (i=8; i;) {
		i--;
		EEARL = 7-i;			// set EPROM Address
		EECR |= (1<<EERE);		// Start eeprom read by writing EERE
		addr[i] =  EEDR;		// Return data from data register
	}
}

#ifdef HAVE_UART
static inline void init_debug(void) { uart_init(UART_BAUD_SELECT(BAUDRATE,F_CPU)); }
#else
#define init_debug()
#endif

/* define __CPU used as name prefix and
 * their uP setup functions, they:
 * - define appropriate clock settings
 * - define the prescaler used the OW_timer
 * - setup the OW_pinchange interrupt
 */
#if defined(__AVR_ATtiny13__)
#define __CPU	AVR_ATtiny13

static inline void AVR_ATtiny13_setup(void)
{
	CLKPR = 0x80;	// Prepare to ...
	CLKPR = 0x00;	// ... set to 9.6 MHz
	TCCR0A = 0;
	TCCR0B = 0x03;	// Prescaler 1/64
	MCUCR |= (1 << ISC00);	// Interrupt on both level changes
}

static inline void AVR_ATtiny13_mask_owpin(void) { GIMSK &= ~(1 << INT0); }
static inline void AVR_ATtiny13_unmask_owpin(void) { GIFR |= (1 << INTF0); GIMSK |= (1 << INT0); }
static inline void AVR_ATtiny13_set_owtimer(timer_t timeout)
{
	TCNT0 = ~timeout;	// overrun at 0xFF
	TIFR0 |= (1 << TOV0);
	TIMSK0 |= (1 << TOIE0);
}
static inline void AVR_ATtiny13_clear_owtimer(void) { TCNT0 = 0; TIMSK0 &= ~(1 << TOIE0); }

// use INT0 pin (PORT B1)
static inline void AVR_ATtiny13_owpin_setup(void) { PORTB &= ~2; DDRB &= ~2; }
static inline void AVR_ATtiny13_owpin_low(void) { DDRB |= 2; }
static inline void AVR_ATtiny13_owpin_hiz(void) { DDRB &= ~2; }
static inline u_char AVR_ATtiny13_owpin_value(void) { return PINB & 2; }

#elif defined (__AVR_ATmega8__)
#define __CPU	AVR_ATmega8

static inline void AVR_ATmega8_setup(void)
{
	// Clock is set via fuse, at least to 8MHz
	TCCR0 = 0x03;			// Prescaler 1/64
	MCUCR |= (1 << ISC00);	// Interrupt on both level changes
}

static inline void AVR_ATmega8_mask_owpin(void) { GIMSK &= ~(1 << INT0); }
static inline void AVR_ATmega8_unmask_owpin(void) { GIFR |= (1 << INTF0); GIMSK |= (1 << INT0); }
static inline void AVR_ATmega8_set_owtimer(timer_t timeout)
{
	TCNT0 = ~timeout;	// overrun at 0xFF
	TIFR |= (1 << TOV0);
	TIMSK |= (1 << TOIE0);
}
static inline void AVR_ATmega8_clear_owtimer(void) { TCNT0 = 0; TIMSK &= ~(1 << TOIE0); }

// use INT0 pin (PORT B2)
static inline void AVR_ATmega8_owpin_setup(void) { PORTB &= ~4; DDRB &= ~4; }
static inline void AVR_ATmega8_owpin_low(void) { DDRB |= 4; }
static inline void AVR_ATmega8_owpin_hiz(void) { DDRB &= ~4; }
static inline u_char AVR_ATmega8_owpin_value(void) { return PINB & 4; }

#elif defined (__AVR_ATmega32__)
#define __CPU	AVR_ATmega32

static inline void AVR_ATmega32_setup(void)
{
	// Clock is set via fuse, at least to 8MHz
	// Clock is set via fuse to 8MHz
	TCCR0 = 0x03;			// Prescaler 1/64
	MCUCR |= (1 << ISC00);	// Interrupt on both level changes
}

static inline void AVR_ATmega32_mask_owpin(void) { GIMSK &= ~(1 << INT0); }
static inline void AVR_ATmega32_unmask_owpin(void) { GIFR |= (1 << INTF0); GIMSK |= (1 << INT0); }
static inline void AVR_ATmega32_set_owtimer(timer_t timeout)
{
	TCNT0 = ~timeout;	// overrun at 0xFF
	TIFR |= (1 << TOV0);
	TIMSK |= (1 << TOIE0);
}
static inline void AVR_ATmega32_clear_owtimer(void) { TCNT0 = 0; TIMSK &= ~(1 << TOIE0); }

// use INT0 pin (PORT D2)
static inline void AVR_ATmega32_owpin_setup(void) { PORTD &= ~4; DDRD &= ~4; }
static inline void AVR_ATmega32_owpin_low(void) { DDRD |= 4; }
static inline void AVR_ATmega32_owpin_hiz(void) { DDRD &= ~4; }
static inline u_char AVR_ATmega32_owpin_value(void) { return PIND & 4; }

#elif defined (__AVR_ATtiny84__)
#define __CPU	AVR_ATtiny84

static inline void AVR_ATtiny84_setup(void)
{
	CLKPR = 0x80;	// Prepare to ...
	CLKPR = 0x00;	// ... set to 8.0 MHz
	MCUCR |= (1 << ISC00);	// Interrupt on both level changes
}

static inline void AVR_ATtiny84_mask_owpin(void) { GIMSK &= ~(1 << INT0); }
static inline void AVR_ATtiny84_unmask_owpin(void) { GIFR |= (1 << INTF0); GIMSK |= (1 << INT0); }
static inline void AVR_ATtiny84_set_owtimer(timer_t timeout)
{
	TCNT0 = ~timeout;	// overrun at 0xFF
	TIFR0 |= (1 << TOV0);
	TIMSK0 |= (1 << TOIE0);
}
static inline void AVR_ATtiny84_clear_owtimer(void) { TCNT0 = 0; TIMSK0 &= ~(1 << TOIE0); }

// use INT0 pin (PORT B2)
static inline void AVR_ATtiny84_owpin_setup(void) { PORTB &= ~4; DDRB &= ~4; }
static inline void AVR_ATtiny84_owpin_low(void) { DDRB |= 4; }
static inline void AVR_ATtiny84_owpin_hiz(void) { DDRB &= ~4; }
static inline u_char AVR_ATtiny84_owpin_value(void) { return PINB & 4; }

#elif defined (__AVR_ATmega168__)
#define __CPU	AVR_ATmega168

static inline void AVR_ATmega168_setup(void)
{
	// Clock is set via fuse, at least to 8MHz
	TCCR0A = 0;
	TCCR0B = 0x03;		// Prescaler 1/64
	EICRA = (1<<ISC00); // interrupt of INT0 (pin D2) on both level changes
}

static inline void AVR_ATmega168_mask_owpin(void) { EIMSK &= ~(1 << INT0); }
static inline void AVR_ATmega168_unmask_owpin(void) { EIFR |= (1 << INTF0); EIMSK |= (1 << INT0); }
static inline void AVR_ATmega168_set_owtimer(timer_t timeout)
{
	TCNT0 = ~timeout;	// overrun at 0xFF
	TIFR0 |= (1 << TOV0);
	TIMSK0 |= (1 << TOIE0);
}
static inline void AVR_ATmega168_clear_owtimer(void) { TCNT0 = 0; TIMSK0 &= ~(1 << TOIE0); }

// use INT0 pin (PORT D2)
static inline void AVR_ATmega168_owpin_setup(void) { PORTD &= ~4; DDRB &= ~4; }
static inline void AVR_ATmega168_owpin_low(void) { DDRD |= 4; }
static inline void AVR_ATmega168_owpin_hiz(void) { DDRD &= ~4; }
static inline u_char AVR_ATmega168_owpin_value(void) { return PIND & 4; }

#else
#error "Your AVR is not supported (or at least not tested)"
#endif


#endif /* AVR_H_ */
