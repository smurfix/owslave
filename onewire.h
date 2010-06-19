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
#include "features.h"

/* Debugging */
#ifdef HAVE_UART
#include "uart.h"

// we should implement some kind of debug level scheme
#define DBG_C(x) uart_putc(x)
#define DBG_P(x) uart_puts_P(x)
#define DBG_N(x) uart_puthex_nibble(x)
#define DBG_X(x) uart_puthex_byte(x)
#define DBG_Y(x) uart_puthex_word(x)
#define DBG_L(x) uart_puthex_long(x)

#define DBG_ONE(str, val) \
	do { DBG_P(str); DBG_X(val); DBG_C('\n'); } while(0)
#define DBG_TWO(str, v1, v2) \
	do { DBG_P(str); DBG_X(v1); DBG_C(','); DBG_X(v2); DBG_C('\n'); } while(0)
// timers may have cpu specific length
#define DBG_TIMER(val) \
	do { DBG_P(#val " = 0x"); switch(sizeof(timer_t)) { \
		case 1: DBG_X((u_char) val); break; \
		case 2: DBG_Y((u_short) val); break; \
		case 4: DBG_L((u_long) val); break; } \
	DBG_C('\n'); } while(0);

#else /* no UART */

#define DBG_C(x)
#define DBG_P(x)
#define DBG_N(x)
#define DBG_X(x)
#define DBG_Y(x)
#define DBG_ONE(str, val)
#define DBG_TWO(str, v1, v2)
#define DBG_TIMER(val)
#endif

/* return to idle state, i.e. wait for the next RESET pulse. */
void set_idle(void);

/* send something. Will return as soon as transmission is active. */
void xmit_bit(u_char bit);
void xmit_byte(u_char bit);
/* receive something. For concurrency, you need to declare your intention
   to receive as soon as possible. Then call recv_bit() or recv_byte()
   when you really need the data. */
void recv_bit(void);
void recv_byte(void);
u_char recv_bit_in(void);
u_char recv_byte_in(void);

/* If you want to do background work, check whether the next unit can be
   sent/received by calling rx_ready() */
u_char rx_ready(void);

void next_idle(void) __attribute__((noreturn));
void next_command(void) __attribute__((noreturn));


/* Incrementally calculate CRC.
   Initially, 'crc' is zero.
   Calculate crc=crc16(crc,byte) for each byte sent/received.
   After sending the last data byte, send crc^0xFFFF (LSB first).
   After receiving data+crc, crc should be 0xB001.
 */
u_short crc16(u_short crc, u_char x);


/************** Your code's Prototypes ****************/

/* Setup */
/* Called before enabling 1wire interrupts */
void init_state(void);

/* Called to process commands. You implement this! */
void do_command(u_char cmd);
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
void update_idle(u_char bits);

#endif
