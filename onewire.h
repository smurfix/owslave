#ifndef ONEWIRE_H
#define ONEWIRE_H

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

#ifdef MAIN
#define EXTERN
#define INIT(x) = x
#else
#define EXTERN extern
#define INIT(x)
#endif

#include <stdint.h>
#include "features.h"


/* Debugging */
#ifdef HAVE_UART
#include "uart.h"

#define DBGS_C(x) uart_putc(x)
#define DBGS_P(x) uart_puts_P(x)
#define DBGS_N(x) uart_puthex_nibble(x)
#define DBGS_X(x) uart_puthex_byte(x)
#define DBGS_Y(x) uart_puthex_word(x)
#define DBGS_NL() uart_putc('\n')

#ifndef HAVE_UART_SYNC
#define DBG_C(x) DBGS_C(x)
#define DBG_P(x) DBGS_P(x)
#define DBG_N(x) DBGS_N(x)
#define DBG_X(x) DBGS_X(x)
#define DBG_Y(x) DBGS_Y(x)
#define DBG_NL() DBGS_NL()
#endif

#ifdef HAVE_TIMESTAMP
volatile unsigned char tbpos;
volatile uint16_t tsbuf[100];
#define DBG_TS(void) do { if(tbpos) tsbuf[--tbpos]=ICR1; } while(0)
#endif

#endif /* UART */

#ifndef DBG_C
#define DBG_C(x) do { } while(0)
#define DBG_P(x) do { } while(0)
#define DBG_N(x) do { } while(0)
#define DBG_X(x) do { } while(0)
#define DBG_Y(x) do { } while(0)
#define DBG_NL() do { } while(0)
#endif

#ifndef DBGS_C
#define DBGS_C(x) do { } while(0)
#define DBGS_P(x) do { } while(0)
#define DBGS_N(x) do { } while(0)
#define DBGS_X(x) do { } while(0)
#define DBGS_Y(x) do { } while(0)
#define DBGS_NL() do { } while(0)
#endif

#ifndef DBG_TS /* signal timestamps. Code does NOT work -- formatting the numbers takes too long */
#define DBG_TS() do { } while(0)
#endif

#ifdef DBGPIN
EXTERN volatile char dbg_interest INIT(0);
#define DBG_IN() do { dbg_interest=1; } while(0)
#define DBG_OUT() do { dbg_interest=0; } while(0)
#define DBG_PIN() (OWPIN & (1<<DBGPIN))
#define DBG_ON() do { if(dbg_interest) OWPORT |= (1<<DBGPIN); } while(0)
#define DBG_OFF() do { if(dbg_interest) OWPORT &= ~(1<<DBGPIN); } while(0)
#define DBG_T(x) do { if(dbg_interest) do { uint8_t _xx=(x); while(_xx) { _xx-=1; DBG_ON();DBG_OFF();} } while(0) } while(0)
#else
#define DBG_IN() do { } while(0)
#define DBG_OUT() do { } while(0)
#define DBG_PIN() 0
#define DBG_ON() do { } while(0)
#define DBG_OFF() do { } while(0)
#define DBG_T(x) do { } while(0)
#endif

/* return to idle state, i.e. wait for the next RESET pulse. */
void set_idle(void);

/* send something. Will return as soon as transmission is active. */
void xmit_bit(uint8_t bit);
void xmit_byte(uint8_t bit);
/* receive something. For concurrency, you need to declare your intention
   to receive as soon as possible. Then call recv_bit() or recv_byte()
   when you really need the data. */
void recv_bit(void);
void recv_byte(void);

uint8_t recv_any_in(void); // don't call directly
static inline uint8_t recv_bit_in(void)
{
    uint8_t byte;
    byte = ((recv_any_in() & 0x80) != 0);
    // DBG_X(byte);
    return byte;
}
static inline uint8_t recv_byte_in(void)
{
    uint8_t byte;
    byte = recv_any_in();
    // DBG_X(byte);
    return byte;
}


uint8_t recv_bit_in(void);
uint8_t recv_byte_in(void);

/* If you want to do background work, check whether the next unit can be
   received by calling rx_ready() */
uint8_t rx_ready(void);

void next_idle(void) __attribute__((noreturn));
void next_command(void) __attribute__((noreturn));


/* Incrementally calculate CRC.
   Initially, 'crc' is zero.
   Calculate crc=crc16(crc,byte) for each byte sent/received.
   After sending the last data byte, send crc^0xFFFF (LSB first).
   After receiving data+crc, crc should be 0xB001.
 */
uint16_t crc16(uint16_t crc, uint8_t x);


/************** Your code's Prototypes ****************/

/* Setup */
/* Called before enabling 1wire interrupts */
void init_state(void);

/* Called to process commands. You implement this! */
void do_command(uint8_t cmd);
/*
   Your code can do any one of:
   * call xmit|recv_bit|byte, as required
   * call next_command() (wait for RESET pulse; will not return)
   * call next_idle() (wait for next command, will not return)

   If you need to run any expensive computations, do it in update_idle().
   Your steps need to be short enough to observe the timing requirements
   of the state you're currently in.

   Do not return.
 */

/* Ditto, but called from idle / bit-wait context. You implement this! */
/* 'bits' says how many 1wire bit times are left. */
void update_idle(uint8_t bits);

#endif
