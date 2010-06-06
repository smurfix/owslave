#include "features.h"
#ifdef HAVE_UART

/*
 *  Copyright © 2008, Matthias Urlichs <matthias@urlichs.de>
 *  Copyright © 2010, Helmut Raiger
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

/* This code is based on work by: */

/*************************************************************************
Title:     Interrupt UART library with receive/transmit circular buffers
Author:    Peter Fleury <pfleury@gmx.ch>   http://jump.to/fleury
File:      $Id: uart.c,v 1.5.2.2 2004/02/27 22:00:28 peter Exp $
Software:  AVR-GCC 3.3 
Hardware:  any AVR with built-in UART, 
           tested on AT90S8515 at 4 Mhz and ATmega at 1Mhz
Extension: uart_puti, uart_puthex by M.Thomas 9/2004

DESCRIPTION:
    An interrupt is generated when the UART has finished transmitting or
    receiving a byte. The interrupt handling routines use circular buffers
    for buffering received and transmitted data.
    
    If POLLED_TRANSMITTER is defined no transmitter interrupt is used
    instead any function that puts characters to the buffer will try
    to get rid of at least one character (see uart_try_send()). The
    application is required to call this function from now and then.
    This way interrupts are never blocked by the transmitter interrupt
    routine, improving real-time behavior.

    The UART_RX_BUFFER_SIZE and UART_TX_BUFFER_SIZE variables define
    the buffer size in bytes. Note that these variables must be a 
    power of 2.
    
USAGE:
    Refere to the header file uart.h for a description of the routines. 
    See also example test_uart.c.

NOTES:
    Based on Atmel Application Note AVR306
                    
LICENSE:
    Copyright (C) 2006 Peter Fleury

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

*************************************************************************/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "uart.h"

/** Size of the circular receive buffer, must be power of 2 */
#ifndef UART_RX_BUFFER_SIZE
#define UART_RX_BUFFER_SIZE 32
#endif
/** Size of the circular transmit buffer, must be power of 2 */
#ifndef UART_TX_BUFFER_SIZE
#define UART_TX_BUFFER_SIZE 512
#endif

/* size of RX/TX buffers */
#define UART_RX_BUFFER_MASK ( UART_RX_BUFFER_SIZE - 1)
#define UART_TX_BUFFER_MASK ( UART_TX_BUFFER_SIZE - 1)

#if ( UART_RX_BUFFER_SIZE & UART_RX_BUFFER_MASK )
 #error "RX buffer size is not a power of 2"
#endif
#if ( UART_TX_BUFFER_SIZE & UART_TX_BUFFER_MASK )
 #error "TX buffer size is not a power of 2"
#endif

/* size of RX/TX buffers */
#define UART_RX_BUFFER_MASK ( UART_RX_BUFFER_SIZE - 1)
#define UART_TX_BUFFER_MASK ( UART_TX_BUFFER_SIZE - 1)

#if ( UART_RX_BUFFER_SIZE & UART_RX_BUFFER_MASK )
#error RX buffer size is not a power of 2
#endif
#if ( UART_TX_BUFFER_SIZE & UART_TX_BUFFER_MASK )
#error TX buffer size is not a power of 2
#endif


/* skip that __AVR__ ifdef hell, guessing names will do
   support for UCSR without A,B drop (very old AVRs)
*/
#ifdef SIG_USART0_RECV
 #define UART0_RECEIVE_INTERRUPT   SIG_USART0_RECV
 #define UART0_TRANSMIT_INTERRUPT  SIG_USART0_DATA
#elif defined(SIG_UART0_RECV)
 #define UART0_RECEIVE_INTERRUPT   SIG_UART0_RECV
 #define UART0_TRANSMIT_INTERRUPT  SIG_UART0_DATA
#elif defined (SIG_USART_RECV)
 /* note: sequence matters here as some parts define USART and UART */
 #define UART0_RECEIVE_INTERRUPT   SIG_USART_RECV
 #define UART0_TRANSMIT_INTERRUPT  SIG_USART_DATA
#elif defined (SIG_UART_RECV)
 #define UART0_RECEIVE_INTERRUPT   SIG_UART_RECV
 #define UART0_TRANSMIT_INTERRUPT  SIG_UART_DATA
#else
 #error "very strange indeed, no interrupts for the UART?"
#endif

#ifdef UDR0
 #define UART0_STATUS   UCSR0A
 #define UART0_CONTROL  UCSR0B
 #define UART0_DATA     UDR0
 #define UART0_UDRIE    UDRIE0
 #define UDRE			UDRE0
#else
 #define UART0_STATUS   UCSRA
 #define UART0_CONTROL  UCSRB
 #define UART0_DATA     UDR
 #define UART0_UDRIE    UDRIE
 /* brute force */
 #define DOR0			DOR
 #define FE0			FE
 #define RXCIE0			RXCIE
 #define RXEN0			RXEN
 #define TXEN0			TXEN
#endif

/* fix the naming for control register C bits */
#ifdef UCSRC
 #define UCSR0C UCSRC
#ifdef URSEL
 #define URSEL0 URSEL
#endif
#ifdef UCSZ0
 #define UCSZ00 UCSZ0
#endif
#endif

/*
 *  module global variables
 */
static volatile unsigned char UART_TxBuf[UART_TX_BUFFER_SIZE];
static volatile unsigned char UART_RxBuf[UART_RX_BUFFER_SIZE];
static volatile unsigned char UART_TxHead;
static volatile unsigned char UART_TxTail;
static volatile unsigned char UART_RxHead;
static volatile unsigned char UART_RxTail;
static volatile unsigned char UART_LastRxError;

/* helper to store a byte from the transmitter ring to the UART TX register
 * turns off transmit interrupts if buffer is empty (just in case
 */
static inline void uart_send(void)
{
	unsigned char tmptail;

	if (UART_TxHead != UART_TxTail) {

		/* calculate and store new buffer index */
		tmptail = (UART_TxTail + 1) & UART_TX_BUFFER_MASK;
		/* get one byte from buffer and write it to UART */
		UART0_DATA = UART_TxBuf[tmptail];  /* start transmission */
		UART_TxTail = tmptail;
	} else
        UART0_CONTROL &= ~_BV(UART0_UDRIE);
}

/* try to empty the transmit buffer, checks if transmitter done first */
void uart_try_send(void)
{
#ifdef POLLED_TRANSMITTER
	if(UART0_STATUS & _BV(UDRE))
		uart_send();
#endif
}

/*************************************************************************

Function: uart_getc()

Purpose:  return byte from ringbuffer  

Returns:  lower byte:  received byte from ringbuffer
          higher byte: last receive error

**************************************************************************/
unsigned int uart_getc(void)
{    
    unsigned char tmptail;
    unsigned char data;


    if (UART_RxHead == UART_RxTail)
        return UART_NO_DATA;   /* no data available */

    /* only block receiver interrupt */
    UART0_CONTROL &= ~_BV(RXCIE0);

    /* calculate /store buffer index */
    tmptail = (UART_RxTail + 1) & UART_RX_BUFFER_MASK;
    UART_RxTail = tmptail; 
    
    /* get data from receive buffer */
    data = UART_RxBuf[tmptail];

    UART0_CONTROL |= _BV(RXCIE0);
    
    return (UART_LastRxError << 8) + data;
}

/*************************************************************************
Function: UART Receive Complete interrupt
Purpose:  called when the UART has received a character
**************************************************************************/
SIGNAL(UART0_RECEIVE_INTERRUPT)
{
    unsigned char tmphead;
    unsigned char data;
    unsigned char usr;
    unsigned char lastRxError;
 
    usr  = UART0_STATUS;
    data = UART0_DATA;
    
    lastRxError = usr & (_BV(FE0) | _BV(DOR0));
        
    /* calculate buffer index */ 
    tmphead = ( UART_RxHead + 1) & UART_RX_BUFFER_MASK;
    
    if ( tmphead != UART_RxTail ) {
		UART_RxBuf[tmphead] = data;
		UART_RxHead = tmphead;
    } else
    	lastRxError = UART_BUFFER_OVERFLOW >> 8;

    UART_LastRxError = lastRxError;   
}

#ifndef POLLED_TRANSMITTER
/*************************************************************************
Function: UART Data Register Empty interrupt
Purpose:  called when the UART is ready to transmit the next byte
**************************************************************************/
SIGNAL(UART0_TRANSMIT_INTERRUPT)
{
	uart_send();
}
#endif


/*************************************************************************
Function: uart_init()
Purpose:  initialize UART and set baudrate
Input:    baudrate using macro UART_BAUD_SELECT()
Returns:  none
**************************************************************************/
void uart_init(unsigned int baudrate)
{
	static unsigned char UART_inited = 0;

	if(UART_inited) return;

	UART_inited = 1;
    UART_TxHead = UART_TxTail = UART_RxHead = UART_RxTail = 0;

    /* set baudrate */
#ifdef UBRRHI
    UBRRHI = (unsigned char) (baudrate>>8);
    UBRR   = (unsigned char) baudrate;
#elif defined(UBRR0H)
    UBRR0H = (unsigned char) (baudrate>>8);
    UBRR0L = (unsigned char) baudrate;
#elif defined(UBRRH)
    UBRRH = (unsigned char) (baudrate>>8);
    UBRRL = (unsigned char) baudrate;
#elif defined(UBRR)
    UBRR = (unsigned char) baudrate;
#else
#error "no baudrate register found"
#endif
    
    /* set a possible control register:
		frame format: asynchronous, 8data, no parity, 2stop bit
	*/
#ifdef UCSR0C
    UCSR0C = (3<<UCSZ00)
#ifdef URSEL0
    | _BV(URSEL0)
#endif
#ifdef USBS0
    | _BV(USBS0)
#endif
    ;
#endif

    /* enable UART receiver and transmmitter and receive complete interrupt */
    UART0_CONTROL = _BV(RXCIE0) | _BV(RXEN0) | _BV(TXEN0);
}

/*************************************************************************
Function: uart_putc()
Purpose:  write byte to ring buffer for transmitting via UART
Input:    byte to be transmitted
Returns:  none          
**************************************************************************/
void uart_putc(unsigned char data)
{
    unsigned char tmphead;

	if(data == '\n')
		uart_putc('\r');

	/* block transmitter interrupts only! */
	UART0_CONTROL &= ~_BV(UART0_UDRIE);

    /* if full drop character, otherwise put to tail */
    tmphead  = (UART_TxHead + 1) & UART_TX_BUFFER_MASK;
    if (tmphead != UART_TxTail) {
		UART_TxBuf[tmphead] = data;
		UART_TxHead = tmphead;
    }

#ifndef POLLED_TRANSMITTER
	/* enable UDRE interrupt again, interrupt turns it off again if empty */
	UART0_CONTROL |= _BV(UART0_UDRIE);
#endif
}


/*************************************************************************
Function: uart_puts()
Purpose:  transmit string to UART
Input:    string to be transmitted
Returns:  none          
**************************************************************************/
void uart_puts(const char *s )
{
    while (*s) 
      uart_putc(*s++);

}/* uart_puts */


/*************************************************************************
Function: uart_puts_p()
Purpose:  transmit string from program memory to UART
Input:    program memory string to be transmitted
Returns:  none
**************************************************************************/
void uart_puts_p(const char *progmem_s )
{
    register char c;
    
    while ( (c = pgm_read_byte(progmem_s++)) ) 
      uart_putc(c);

}/* uart_puts_p */

#if 0 /* unused */
/*************************************************************************
Function: uart_puti()
Purpose:  transmit integer as ASCII to UART
Input:    integer value
Returns:  none
This functions has been added by Martin Thomas <eversmith@heizung-thomas.de>
Don't blame P. Fleury if it doesn't work ;-)
**************************************************************************/
void uart_puti( const int val )
{
    char buffer[sizeof(int)*8+1];
    uart_puts( itoa(val, buffer, 10) );
}/* uart_puti */
void uart_putl( const long val )
{
    char buffer[sizeof(long)*8+1];
    uart_puts( ltoa(val, buffer, 10) );
}/* uart_puti */
#endif

/*************************************************************************
Function: uart_puthex_nibble()
Purpose:  transmit lower nibble as ASCII-hex to UART
Input:    byte value
Returns:  none
This functions has been added by Martin Thomas <eversmith@heizung-thomas.de>
Don't blame P. Fleury if it doesn't work ;-)
**************************************************************************/
void uart_puthex_nibble(const unsigned char b)
{
    unsigned char  c = b & 0x0f;
    if (c>9) c += 'A'-10;
    else c += '0';
    uart_putc(c);
} /* uart_puthex_nibble */

/*************************************************************************
Function: uart_puthex_byte()
Purpose:  transmit upper and lower nibble as ASCII-hex to UART
Input:    byte value
Returns:  none
This functions has been added by Martin Thomas <eversmith@heizung-thomas.de>
Don't blame P. Fleury if it doesn't work ;-)
**************************************************************************/
void uart_puthex_byte_(const unsigned char  b)
{
    uart_puthex_nibble(b>>4);
    uart_puthex_nibble(b);
}
void uart_puthex_byte(const unsigned char  b)
{
    if(b & 0xF0)
        uart_puthex_nibble(b>>4);
    uart_puthex_nibble(b);
} /* uart_puthex_byte */
/*************************************************************************
Function: uart_puthex_word()
Purpose:  transmit upper and lower byte as ASCII-hex to UART
Input:    word value
Returns:  none
This functions has been added by Matthias Urlichs.
Don't blame P. Fleury if it doesn't work ;-)
**************************************************************************/
void uart_puthex_word(const unsigned short b)
{
    if (b&0xFF00) {
        uart_puthex_byte(b>>8);
        uart_puthex_byte_(b);
    } else {
        uart_puthex_byte(b);
    }
} /* uart_puthex_word */
#else
void uart_try_send(void) {}
#endif // HAVE_UART
