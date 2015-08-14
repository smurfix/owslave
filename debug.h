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

#ifndef NO_DEBUG //

#ifdef UART_DEBUG
#ifndef HAVE_UART
#error "Cannot debug using UART without having one"
#endif
#include "uart.h"

/* These macros always send something. They should only be used in
 * sync-safe places, i.e. not in interrupt code or whatever.
 */
#define DBG_C(x) uart_putc(x)
#define DBG_S(x) uart_puts(x)
#define DBG_P(x) uart_puts_P(x)
#define DBG_P_(x) uart_puts_p(x)
#define DBG_N(x) uart_puthex_nibble(x)
#define DBG_W(x) uart_puthex_word(x)
#define DBG_X(x) uart_puthex_byte_(x)
#define DBG_NL() uart_putc('\n')
#endif /* UART_DEBUG */

#ifdef CONSOLE_DEBUG
#ifdef DBG_C
#error "Cannot use two debug methods concurrently"
#endif
#ifndef N_CONSOLE
#error "Cannot debug using console without having one"
#endif
#include "console.h"

#define DBG_C(x) console_putc(x)
#define DBG_S(x) console_puts(x)
#define DBG_P(x) console_puts_P(x)
#define DBG_P_(x) console_puts_p(x)
#define DBG_N(x) console_puthex_nibble(x)
#define DBG_W(x) console_puthex_word(x)
#define DBG_X(x) console_puthex_byte_(x)
#define DBG_NL() console_putc('\n')
#endif // console

#endif // !NO_DEBUG

#ifndef DBG_C
#define DBG_C(x) do { } while(0)
#define DBG_S(x) do { } while(0)
#define DBG_P(x) do { } while(0)
#define DBG_P_(x) do { } while(0)
#define DBG_N(x) do { } while(0)
#define DBG_W(x) do { } while(0)
#define DBG_X(x) do { } while(0)
#define DBG_NL() do { } while(0)
#endif

//************* second part: debugging via emitting a bit/byte to a port

#if defined(HAVE_DBG_PORT) && !defined(NO_DEBUG)
#define DBGA(x,v) do { v=(x); asm volatile("out %0,%1" :: "i"(((int)&DBGPORT)-__SFR_OFFSET),"r"(v)); } while(0)
#define DBG(x) do { uint8_t _x; DBGA(x,_x); } while(0)
#else
#define DBG(x) do { } while(0)
#define DBGA(x,v) do { } while(0)
#undef DBGPORT
#endif

#if defined(HAVE_DBG_PIN) && !defined(NO_DEBUG)
#define DBG_ON() do { DBGPINPORT |= (1<<DBGPIN); } while(0)
#define DBG_OFF() do { DBGPINPORT &= ~(1<<DBGPIN); } while(0)
#else
#define DBG_ON() do { } while(0)
#define DBG_OFF() do { } while(0)
#undef DBGPINPORT
#endif

#endif // debug_h
