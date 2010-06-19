/*
 * cortexm0.h
 *   only included if compiling for a Cortex M0 cpu
 */

#ifndef CORTEXM0_H_
#define CORTEXM0_H_

#include "uart.h"

#ifdef __USE_CMSIS
#include "LPC11xx.h"
#endif
#define __CPU	CortexM0

// Cortex M0 uses a 32bit timer
typedef unsigned long timer_t;

/* timer setup for Cortex M0 at 50MHz is less critical:
 *   - interrupt latency is about 12cycles (240ns)
 *   - timer resolution is about 20nsec
 */
#define T_(c) ((c)*50)
#define T_PRESENCE T_(120)
#define T_PRESENCEWAIT T_(20)
#define T_RESET_ T_(400)
#define T_RESET (T_RESET_-T_SAMPLE)
#define T_SAMPLE T_(15)
#define T_XMIT T_(60)

#ifdef HAVE_UART
static inline void init_debug(void) { uart_init(BAUDRATE); }
#else
#define init_debug()
#endif


/*!
 * currently these are NO-OPs, compilability test only!
 */
static inline void CortexM0_setup(void)
{
}

static inline void CortexM0_mask_owpin(void) { }
static inline void CortexM0_unmask_owpin(void) { }
static inline void CortexM0_set_owtimer(timer_t timeout)
{
}
static inline void CortexM0_clear_owtimer(void) { }

//
static inline void CortexM0_owpin_setup(void) {  }
static inline void CortexM0_owpin_low(void) { }
static inline void CortexM0_owpin_hiz(void) { }
static inline u_char CortexM0_owpin_value(void) { return 1; }

// not really
#define OW_TIMER_ISR() void TIMER32_0_IRQHandler(void)
#define OW_PINCHANGE_ISR() void PIOINT0_IRQHandler(void)

// generic
#define cli()
#define sei()
// get silicon ID of the chip and calculate the CRC-8 here
static inline void get_ow_address(u_char *addr) {}

static volatile u_char TCNT0;


#endif /* CORTEXM0_H_ */
