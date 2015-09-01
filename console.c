
/*
 *  Copyright © 2008-2015, Matthias Urlichs <matthias@urlichs.de>
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
#ifdef N_CONSOLE

#include <stdlib.h>
#include "pgm.h"
#include <stdlib.h>
#include "console.h"
#include "debug.h"

/** Size of the circular transmit buffer, must be power of 2 */
#ifndef CONSOLE_BUFFER_SIZE
#define CONSOLE_BUFFER_SIZE 128
#endif
#define CONSOLE_BUFFER_MASK ((CONSOLE_BUFFER_SIZE)-1)

#if (CONSOLE_BUFFER_SIZE & CONSOLE_BUFFER_MASK)
#error TX buffer size is not a power of 2
#endif

/*
 *  module global variables
 */
#ifndef DBG_CONSOLE_SYNC
static volatile unsigned char console_buf[CONSOLE_BUFFER_SIZE];
static volatile unsigned char console_head;
static volatile unsigned char console_tail;
#endif

// initialized early, thus not called init_console()
void console_init(void)
{
	console_head = 0;
	console_tail = 0;
}

void console_putc(unsigned char data)
{
	uint8_t sreg;
	uint8_t head,head2;

	sreg = SREG;
	cli();

	head = console_head;
	head2 = (head+1) & CONSOLE_BUFFER_MASK;
	if (head2 != console_tail) {
		console_buf[head] = data;
		console_head = head2;
	} else {
		/* Mark overrun by a null byte */
		head = (head-1) & CONSOLE_BUFFER_MASK;
		console_buf[head] = 0x00;
	}

	SREG = sreg;
}

uint8_t console_buf_len(void)
{
	return (console_head-console_tail) & CONSOLE_BUFFER_MASK;
}

uint8_t console_buf_read(unsigned char *addr, uint8_t maxlen)
{
	uint8_t head,tail,len = 0;
	
	head = console_head;
	tail = console_tail;
	while(head != tail && len < maxlen) {
		*addr++ = console_buf[tail];
		len++;
		tail = (tail+1) & CONSOLE_BUFFER_MASK;
	}
	return len;
}

void console_buf_done(uint8_t len)
{
	console_tail = (console_tail + len) & CONSOLE_BUFFER_MASK;
}

void console_puts(const char *s)
{
	while (*s) 
	  console_putc(*s++);
}

void console_puts_p(const char *progmem_s)
{
	register char c;
	
	while ( (c = pgm_read_byte(progmem_s++)) ) 
	  console_putc(c);
}

#if 0 /* unused */
void console_puti(const int val)
{
	char buffer[sizeof(int)*8+1];
	console_puts(itoa(val, buffer, 10));
}
void console_putl(const long val)
{
	char buffer[sizeof(long)*8+1];
	console_puts( ltoa(val, buffer, 10));
}
#endif

void console_puthex_nibble(const unsigned char b)
{
	unsigned char c = b & 0x0f;
	if (c>9) c += 'A'-10;
	else c += '0';
	console_putc(c);
}

void console_puthex_byte_(const unsigned char b)
{
	console_puthex_nibble(b>>4);
	console_puthex_nibble(b);
}

void console_puthex_byte(const unsigned char b)
{
	if(b & 0xF0)
		console_puthex_nibble(b>>4);
	console_puthex_nibble(b);
}

void console_puthex_word(const uint16_t b)
{
	if (b&0xFF00) {
		console_puthex_byte(b>>8);
		console_puthex_byte_(b);
	} else {
		console_puthex_byte(b);
	}
}

#endif // HAVE_CONSOLE
