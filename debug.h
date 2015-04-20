#ifndef debug_h
#define debug_h

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

#include "dev_config.h"

/* Debugging */
#ifdef HAVE_UART
#include "uart.h"

/* These macros always send something. They should only be used in
 * sync-safe places, i.e. not in interrupt code or whatever.
 */
#define DBGS_C(x) uart_putc(x)
#define DBGS_P(x) uart_puts_P(x)
#define DBGS_N(x) uart_puthex_nibble(x)
#define DBGS_X(x) uart_puthex_byte_(x)
#define DBGS_Y(x) uart_puthex_word(x)
#define DBGS_NL() uart_putc('\n')

#ifndef HAVE_UART_SYNC
/* These macros can be used anywhere. They'll turn into no-ops when
 * synchronous output is tuned on.
 */
#define DBG_C(x) DBGS_C(x)
#define DBG_P(x) DBGS_P(x)
#define DBG_N(x) DBGS_N(x)
#define DBG_X(x) DBGS_X(x)
#define DBG_Y(x) DBGS_Y(x)
#define DBG_NL() DBGS_NL()
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

#ifdef HAVE_DBG_PORT
EXTERN volatile char dbg_interest INIT(0);
#define DBGA(x,v) do { v=(x); asm volatile("out %0,%1" :: "i"(((int)&DBGPORT)-__SFR_OFFSET),"r"(v)); } while(0)
#define DBG(x) do { uint8_t _x; DBGA(x,_x); } while(0)
#else
#define DBG(x) do { } while(0)
#define DBGA(x,v) do { } while(0)
#endif

#ifdef HAVE_DBG_PIN
#define DBG_ON() do { DBGPINPORT |= (1<<DBGPIN); } while(0)
#define DBG_OFF() do { DBGPINPORT &= ~(1<<DBGPIN); } while(0)
#else
#define DBG_ON() do { } while(0)
#define DBG_OFF() do { } while(0)
#endif

#endif // debug_h
