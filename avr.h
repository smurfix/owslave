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
static inline void AVR_ATtiny13_set_owtimer(u_char timeout)
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
static inline void AVR_ATmega8_set_owtimer(u_char timeout)
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
static inline void AVR_ATmega32_set_owtimer(u_char timeout)
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
static inline void AVR_ATtiny84_set_owtimer(u_char timeout)
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
static inline void AVR_ATmega168_set_owtimer(u_char timeout)
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
