/*
 * cortexm0.h
 *   only included if compiling for a Cortex M0 cpu
 */

#ifndef CORTEXM0_H_
#define CORTEXM0_H_

#include <string.h>

#ifdef __USE_CMSIS
#include "LPC11xx.h"
#endif
#define __CPU	CortexM0

/* these are HW specific and should be configurable, take care
 * 	  if number is wrong, it simply won't be the interrupt handler,
 *    but no error is issued.
 */
#define USE_TIMER		1			/* 0 or 1 */
#define USE_PORT		2			/* 0, 1, 2, 3 */
#define USE_PIN			0			/* 0 .. 7 */

#define PRESCALE	1

// Cortex M0 uses a 32bit timer
typedef unsigned long timer_t;

/* timer setup for Cortex M0 at >36MHz is less critical:
 *   - interrupt latency is about 12cycles (350ns)
 *   - timer resolution should be better than 100nsec
 *
 * TODO: this needs further approval
 */
#define T_(c) ((F_CPU/PRESCALE)/(1000000/c))
#define T_PRESENCE T_(120)
#define T_PRESENCEWAIT T_(40)
#define T_RESET_ T_(400)
#define T_RESET (T_RESET_-T_SAMPLE)
#define T_SAMPLE T_(30)
#define T_XMIT T_(30)

#ifdef HAVE_UART
static inline void init_debug(void) { uart_init(BAUDRATE); }
#else
#define init_debug()
#endif

#define _COMPOSE2(a, b)			a ## b
#define _COMPOSE3(a, b, c)		a ## b ## c

static inline void ow_pinchange_isr(void);

// pin interrupt stuff composition
#define _PIOINT(port)		_COMPOSE3(PIOINT, port, _IRQHandler)
#define REAL_PIN_ISR()		void _PIOINT(USE_PORT)(void)
#define _EINT(port)			_COMPOSE3(EINT, port, _IRQn)
#define EINT_IRQn			_EINT(USE_PORT)
// LPC_GPIO register composition
#define _LPC_GPIO(port)		_COMPOSE2(LPC_GPIO, port)
#define LPC_GPIO			(_LPC_GPIO(USE_PORT))
#define PIN_MASK			(1 << USE_PIN)
#define OW_PINCHANGE_ISR()	void ow_pinchange_isr(void)

// timer interrupt composition
#define _TIMER32(timer)	_COMPOSE3(TIMER32_, timer, _IRQHandler)
#define OW_TIMER_ISR()	void _TIMER32(USE_TIMER)(void)
#define _TINT(timer)	_COMPOSE3(TIMER_32_, timer, _IRQn)
#define TIMER_32_IRQn	_TINT(USE_TIMER)

// LPC_TMR32Bx register composition
#define _LPC_TMR32B(timer)	_COMPOSE2(LPC_TMR32B, timer)
#define LPC_TMR32B			(_LPC_TMR32B(USE_TIMER))

// a few timer bits
#define IR_MR0			(1 << 0)	// interrupt match 0  (w1c)
#define TCR_CE			(1 << 0)	// timer/counter enable
#define TCR_CR			(1 << 1)	// clear synchronously
#define MCR_MR0I		(1 << 0)	// allow interrupt on match 0
#define MCR_MR0R		(1 << 1)	// reset timer on match 0
#define MCR_MR0S		(1 << 2)	// stop timer on match 0

/*!
 *  Setup cpu specifics for the LPC111x:
 *    hardware setup:
 *    	- define the pin to use (any can generate interrupts), P0_7 has higher drive capabilities
 *      - use 32bit timer (16bit would do, but who cares), both timer 0 and 1 can be used
 *
 *  GPIO interrupts are port specific (0..3), if the application needs an additional pin interrupt
 *  from the same port we need to share the ISR (TODO).
 *  TODO set-up core clock for optimum flash performance and just enough resolution for the timer
 */
static inline void CortexM0_setup(void)
{
	// set-up 32 bit timer (timer 0 -> bit 9, timer 1 -> bit 10)
	LPC_SYSCON->SYSAHBCLKCTRL |= 1 << (USE_TIMER+9);

	// divide by PR+1
	LPC_TMR32B->PR = PRESCALE-1;
	// enable but reset
	LPC_TMR32B->TCR = TCR_CE | TCR_CR;
	LPC_TMR32B->MR0 = 0xffffffff;
	// interrupt on match, stop timer and clear it
	LPC_TMR32B->MCR = MCR_MR0I | MCR_MR0S | MCR_MR0R;
	// allow interrupts from there
	NVIC_EnableIRQ(TIMER_32_IRQn);
	// ack interrupt and run
	LPC_TMR32B->IR |= IR_MR0;
	LPC_TMR32B->TCR &= ~TCR_CR;
}

/* Setup pin as input with pull-up which is
 * actually external, but only if connected
 */
static inline void CortexM0_owpin_setup(void)
{
	// Enable AHB clock to the GPIO domain.
	LPC_SYSCON->SYSAHBCLKCTRL |= 1 << 6;

	// set low, but as input
	LPC_GPIO->MASKED_ACCESS[PIN_MASK] = 0;
	LPC_GPIO->DIR &= ~PIN_MASK;

	// edge triggered, both edges
	LPC_GPIO->IS &= ~PIN_MASK;
	LPC_GPIO->IBE |= PIN_MASK;
	// enable interrupt (pin not unmasked yet!)
	NVIC_EnableIRQ(EINT_IRQn);
}

// actual interrupt handler to call ow_pinchange_isr()
REAL_PIN_ISR()
{
	if(LPC_GPIO->MIS & PIN_MASK)
		ow_pinchange_isr();
	LPC_GPIO->IC |= PIN_MASK;
}

// only switch to output
static inline void CortexM0_owpin_low(void) { LPC_GPIO->DIR |= PIN_MASK; }
// only switch to input
static inline void CortexM0_owpin_hiz(void) { LPC_GPIO->DIR &= ~PIN_MASK; }
// disable port interrupts (all!)
static inline void CortexM0_mask_owpin(void) { LPC_GPIO->IE &= ~PIN_MASK; }
// enable port interrupts (all!)
static inline void CortexM0_unmask_owpin(void) { LPC_GPIO->IE |= PIN_MASK; }
static inline u_char CortexM0_owpin_value(void)
{
	return (u_char) LPC_GPIO->MASKED_ACCESS[PIN_MASK];
}

static inline void _set_timer_match(u_long timeout)
{
	LPC_TMR32B->TCR |= TCR_CE | TCR_CR;	// start but keep counter reset
	LPC_TMR32B->MR0 = timeout;			// match on overflow
	LPC_TMR32B->TCR &= ~TCR_CR;			// counter run (should be about 100nsec later)
}

/* set the OW timer, so that it generates an interrupt in timeout ticks */
static inline void CortexM0_set_owtimeout(timer_t timeout)
{
	_set_timer_match(timeout);
}

/* reset OW timer (to 0) */
static inline void CortexM0_clear_owtimer(void)
{
	// clear the match interrupt here as it is called in OW_TIMER_ISR
	_set_timer_match(0xffffffff);
	LPC_TMR32B->IR |= IR_MR0;
}

/* current timer run value */
static inline timer_t CortexM0_owtimer(void) { return LPC_TMR32B->TC; }

/* return true if owtimer timeout is about <1.5 bit times = 90usec */
static inline int CortexM0_owtimer_is_set_to_short_timeout(void)
{
	return LPC_TMR32B->MR0 < T_(90);
}

// generic
#define cli() __disable_irq()
#define sei() __enable_irq()

// get silicon ID of the chip and calculate the CRC-8 here
// currently a precalculated one, reversed order!
static inline void get_ow_address(u_char *addr)
{
	strcpy((char *) addr, "\x8D\x42\xBE\x26\x4C\x51\xD8\x29");
}


#endif /* CORTEXM0_H_ */
