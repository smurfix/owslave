#ifndef ONEWIRE_H
#define ONEWIRE_H

/*
 *  Copyright © 2010-2015, Matthias Urlichs <matthias@urlichs.de>
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

#include <stdint.h>
#include "features.h"

#ifdef HAVE_ONEWIRE
/* return to idle state, i.e. wait for the next RESET pulse. */
void set_idle(void);

/* aborts and return to idle state */
#ifdef ONE_WIRE_DEBUG
void next_idle(char reason) __attribute__((noreturn));
#else
#define next_idle(x) _next_idle()
void _next_idle(void) __attribute__((noreturn));
#endif

/* send something. Will return as soon as transmission is active. */
void xmit_bit(uint8_t bit);
void xmit_byte(uint8_t bit);
uint16_t xmit_byte_crc(uint16_t crc, uint8_t val);
uint16_t xmit_bytes_crc(uint16_t crc, uint8_t *buf, uint8_t len);

/* receive something. For concurrency, you need to declare your intention
   to receive as soon as possible. Then call recv_bit() or recv_byte()
   when you really need the data. */
void recv_bit(void);
void recv_byte(void);
uint16_t recv_bytes_crc(uint16_t crc, uint8_t *buf, uint8_t len);

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

/* If you want to do background work, check whether the next unit can be
   received by calling rx_ready() */
uint8_t rx_ready(void);

void next_command(void) __attribute__((noreturn));

/* Set up onewire-specific hardware */
void onewire_init(void);

/* Poll the bus. Will not return while a transaction is in progress. */
void onewire_poll(void);

#else /* !HAVE_ONEWIRE */
#define onewire_init() do {} while(0)
#define onewire_poll() do {} while(0)

#endif

#endif // onewire.h
