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

/*
 * This code emits a number of flashes on the debug pin, corresponding
 * to the interrupt which would otherwise have reset your IC because
 * you forgot to implement a handler for it.
 */
#include "features.h"

#ifdef HAVE_IRQ_CATCHER

#include "debug.h"
#include "uart.h"
#include <avr/wdt.h>

uint8_t reboot_irq __attribute__ ((section (".noinit")));

void ping_me(uint8_t c) {
	asm("eor r1,r1"); // we don't know where we came from
	cli();
    DBG_OFF();DBG_ON();DBG_OFF();DBG_ON();DBG_OFF();
    DBG(c);
    DBG_ON();DBG_OFF(); DBG_ON();DBG_OFF();
#if defined(HAVE_WATCHDOG)
	wdt_reset();
#endif
#ifdef HAVE_UART_SYNC
	uart_puts_P("\nDEAD x");
	uart_puthex_byte(c);
	uart_putc('\n');
	uart_putc(' ');
#endif
	wdt_enable(0); // (almost) immediate
    do {} while(1);
}

// These reset anyway, so don't bother with niceties
void __vector_1(void) __attribute__((naked));
void __vector_2(void) __attribute__((naked));
void __vector_3(void) __attribute__((naked));
void __vector_4(void) __attribute__((naked));
void __vector_5(void) __attribute__((naked));
void __vector_6(void) __attribute__((naked));
void __vector_7(void) __attribute__((naked));
void __vector_8(void) __attribute__((naked));
void __vector_9(void) __attribute__((naked));
void __vector_10(void) __attribute__((naked));
void __vector_11(void) __attribute__((naked));
void __vector_12(void) __attribute__((naked));
void __vector_13(void) __attribute__((naked));
void __vector_14(void) __attribute__((naked));
void __vector_15(void) __attribute__((naked));
void __vector_16(void) __attribute__((naked));
void __vector_17(void) __attribute__((naked));
void __vector_18(void) __attribute__((naked));
void __vector_19(void) __attribute__((naked));
void __vector_20(void) __attribute__((naked));
void __vector_21(void) __attribute__((naked));
void __vector_22(void) __attribute__((naked));
void __vector_23(void) __attribute__((naked));
void __vector_24(void) __attribute__((naked));
void __vector_25(void) __attribute__((naked));
void __vector_26(void) __attribute__((naked));
void __vector_27(void) __attribute__((naked));
void __vector_28(void) __attribute__((naked));
void __vector_29(void) __attribute__((naked));
void __vector_30(void) __attribute__((naked));
void __vector_31(void) __attribute__((naked));

#if defined(HAVE_TIMER) && !defined(HAVE_TOV0)
#define HAVE_TOV0
#endif

#ifdef HAVE_ONEWIRE
#if defined (__AVR_ATmega168__) || defined (__AVR_ATmega88__) || defined(__AVR_ATmega328__)
#define HAVE_TOV2
#else
#define HAVE_TOV0
#endif
#endif

#if !defined(ONEWIRE_IRQNUM) || ONEWIRE_IRQNUM != -1
void __vector_1(void) { ping_me(1); } // INT0
#endif
#if !defined(ONEWIRE_IRQNUM) || ONEWIRE_IRQNUM != -2
void __vector_2(void) { ping_me(2); }
#endif
void __vector_3(void) { ping_me(3); }
void __vector_4(void) { ping_me(4); }
void __vector_5(void) { ping_me(5); }
void __vector_6(void) { ping_me(6); }
void __vector_7(void) { ping_me(7); }
void __vector_8(void) { ping_me(8); }
#ifndef HAVE_TOV2
void __vector_9(void) { ping_me(9); }
#endif
void __vector_10(void) { ping_me(10); }
void __vector_11(void) { ping_me(11); }
void __vector_12(void) { ping_me(12); }
void __vector_13(void) { ping_me(13); }
void __vector_14(void) { ping_me(14); }
void __vector_15(void) { ping_me(15); }
#ifndef HAVE_TIMER
void __vector_16(void) { ping_me(16); }
#endif
void __vector_17(void) { ping_me(17); }
#ifndef HAVE_UART_IRQ
void __vector_18(void) { ping_me(18); }
void __vector_19(void) { ping_me(19); }
#endif
void __vector_20(void) { ping_me(20); }
void __vector_21(void) { ping_me(21); }
void __vector_22(void) { ping_me(22); }
void __vector_23(void) { ping_me(23); }
void __vector_24(void) { ping_me(24); }
void __vector_25(void) { ping_me(25); }
void __vector_26(void) { ping_me(26); }
void __vector_27(void) { ping_me(27); }
void __vector_28(void) { ping_me(28); }
void __vector_29(void) { ping_me(29); }
void __vector_30(void) { ping_me(30); }
void __vector_31(void) { ping_me(31); }

#endif

