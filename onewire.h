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
#else
#define EXTERN extern
#endif

#include <stdint.h>
#include "features.h"


/* Debugging */
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

#else /* no UART */

#define DBG_C(x) do { } while(0)
#define DBG_P(x) do { } while(0)
#define DBG_N(x) do { } while(0)
#define DBG_X(x) do { } while(0)
#define DBG_Y(x) do { } while(0)
#define DBG_NL() do { } while(0)
#endif

#ifndef DBG_TS /* signal timestamps. Code does NOT work -- formatting the numbers takes too long */
#define DBG_TS() do { } while(0)
#endif


/* State machine. */
EXTERN volatile uint8_t state;

/* return to idle state, i.e. wait for the next RESET pulse. */
void set_idle(void);

// Basic bus state machine
//  Bitmasks
#define S_RECV 0x01
#define S_XMIT 0x02
#define S_MASK 0x7F
#define S_XMIT2 0x80 // flag to de-assert zero bit on xmit timeout

//  initial states: >3 byte times
#define S_IDLE            (       0x00) // wait for Reset
#define S_RESET           (       0x04) // Reset seen
#define S_PRESENCEPULSE   (       0x08) // sending Presence pulse
//  selection opcode states: 1 byte times
#define S_RECEIVE_ROMCODE (S_RECV|0x10) // reading selection opcode
#define S_MATCHROM        (S_RECV|0x14) // select a known slave
#define S_READROM         (S_XMIT|0x14) // single slave only!

#ifndef SKIP_SEARCH
#define S_SEARCHROM       (S_XMIT|0x18) // search, step 1: send ID bit
#define S_SEARCHROM_I     (S_XMIT|0x1C) // search, step 2: send inverted ID bit
#define S_SEARCHROM_R     (S_RECV|0x18) // search, step 3: check what the master wants
#endif
//  opcode states: 1 bit time
#define S_RECEIVE_OPCODE  (S_RECV|0x20) // reading real opcode
#define S_HAS_OPCODE      (       0x24) // has real opcode, mainloop
#define S_CMD_RECV        (S_RECV|0x28) // receive bytes
#define S_CMD_XMIT        (S_XMIT|0x28) // send bytes
#define S_CMD_IDLE        (       0x28) // do nothing

#define S_IN_APP(x) ((x)&0x20) // in "application" state

/* send something. Will return as soon as transmission is active. */
void xmit_bit(uint8_t bit);
void xmit_byte(uint8_t bit);
/* receive something. For concurrency, you need to declare your intention
   to receive as soon as possible. Then call recv_bit() or recv_byte()
   when you really need the data. */
void recv_bit(void);
void recv_byte(void);
uint8_t recv_bit_in(void);
uint8_t recv_byte_in(void);

/* If you want to do background work, check whether the next unit can be
   sent/received by calling rx_ready() */
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
