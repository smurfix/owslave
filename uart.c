#include "features.h"
#ifdef HAVE_UART

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
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "pgm.h"
#include <stdlib.h>
#include <stdio.h>

#include "dev_data.h"
#ifndef DEBUG_UART
#define NO_DEBUG
#endif
#include "debug.h"
#include "uart.h"

/** Size of the circular receive buffer, must be power of 2 */
#ifndef UART_RX_BUFFER_SIZE
#define UART_RX_BUFFER_SIZE 8
#endif
/** Size of the circular transmit buffer, must be power of 2 */
#ifndef UART_TX_BUFFER_SIZE
#define UART_TX_BUFFER_SIZE 128
#endif


/*
 *  constants and macros
 */

/* size of RX/TX buffers */
#define UART_RX_BUFFER_MASK ( UART_RX_BUFFER_SIZE - 1)
#define UART_TX_BUFFER_MASK ( UART_TX_BUFFER_SIZE - 1)

#if ( UART_RX_BUFFER_SIZE & UART_RX_BUFFER_MASK )
#error RX buffer size is not a power of 2
#endif
#if ( UART_TX_BUFFER_SIZE & UART_TX_BUFFER_MASK )
#error TX buffer size is not a power of 2
#endif

#if defined(__AVR_AT90S2313__) \
 || defined(__AVR_AT90S4414__) || defined(__AVR_AT90S4434__) \
 || defined(__AVR_AT90S8515__) || defined(__AVR_AT90S8535__)
 /* old AVR classic with one UART */
 #define AT90_UART
 #define UART0_RECEIVE_INTERRUPT   SIG_UART_RECV
 #define UART0_TRANSMIT_INTERRUPT  SIG_UART_DATA
 #define UART0_STATUS   USR
 #define UART0_CONTROL  UCR
 #define UART0_DATA     UDR  
 #define UART0_UDRIE    UDRIE
#elif defined(__AVR_AT90S2333__) || defined(__AVR_AT90S4433__)
 /* old AVR classic with one UART */
 #define AT90_UART
 #define UART0_RECEIVE_INTERRUPT   UART_RX_vect
 #define UART0_TRANSMIT_INTERRUPT  UART_UDRE_vect
 #define UART0_STATUS   UCSRA
 #define UART0_CONTROL  UCSRB
 #define UART0_DATA     UDR 
 #define UART0_UDRIE    UDRIE
 #define UART0_ERRMASK (_BV(FE)|_BV(DOR))
#elif  defined(__AVR_ATmega8__)  || defined(__AVR_ATmega16__) || defined(__AVR_ATmega32__) \
  || defined(__AVR_ATmega8515__) || defined(__AVR_ATmega8535__) \
  || defined(__AVR_ATmega323__) 
  /* ATMega with one USART */
 #define ATMEGA_USART
 #define UART0_RECEIVE_INTERRUPT   USART_RXC_vect
 #define UART0_TRANSMIT_INTERRUPT  USART_UDRE_vect
 #define UART0_STATUS   UCSRA
 #define UART0_CONTROL  UCSRB
 #define UART0_DATA     UDR
 #define UART0_UDRIE    UDRIE
 #define UART0_ERRMASK (_BV(FE)|_BV(DOR))
#elif defined(__AVR_ATmega163__) 
  /* ATMega163 with one UART */
 #define ATMEGA_UART
 #define UART0_RECEIVE_INTERRUPT   UART_RX_vect
 #define UART0_TRANSMIT_INTERRUPT  UART_UDRE_vect
 #define UART0_STATUS   UCSRA
 #define UART0_CONTROL  UCSRB
 #define UART0_DATA     UDR
 #define UART0_UDRIE    UDRIE
 #define UART0_ERRMASK (_BV(FE)|_BV(DOR))
#elif defined(__AVR_ATmega162__) || defined(__AVR_ATmega168__) || defined(__AVR_ATmega88__) || defined (__AVR_ATmega328__)
 /* ATMega with two USART */
 #define ATMEGA_USART0
 #define UART0_RECEIVE_INTERRUPT   USART_RX_vect
 #define UART0_TRANSMIT_INTERRUPT  USART_UDRE_vect
 #define UART0_STATUS   UCSR0A
 #define UART0_CONTROL  UCSR0B
 #define UART0_DATA     UDR0
 #define UART0_UDRIE    UDRIE0
 #define UART0_ERRMASK (_BV(FE0)|_BV(DOR0))
#elif defined(__AVR_ATmega64__) || defined(__AVR_ATmega128__) 
 /* ATMega with two USART */
 #define ATMEGA_USART0
 #define UART0_RECEIVE_INTERRUPT   USART0_RX_vect
 #define UART0_TRANSMIT_INTERRUPT  USART0_UDRE_vect
 #define UART0_STATUS   UCSR0A
 #define UART0_CONTROL  UCSR0B
 #define UART0_DATA     UDR0
 #define UART0_UDRIE    UDRIE0
#else
 #error "Your CPU is not yet supported by this libaray!"
#endif


/*
 *  module global variables
 */
#ifndef DBG_UART_SYNC
static volatile unsigned char UART_TxBuf[UART_TX_BUFFER_SIZE];
static volatile unsigned char UART_RxBuf[UART_RX_BUFFER_SIZE];
static volatile unsigned char UART_TxHead;
static volatile unsigned char UART_TxTail;
static volatile unsigned char UART_RxHead;
static volatile unsigned char UART_RxTail;
#endif
static volatile unsigned char UART_LastRxError;


/*************************************************************************

Function: uart_getc()

Purpose:  return byte from ringbuffer  

Returns:  lower byte:  received byte from ringbuffer
          higher byte: last receive error

**************************************************************************/

unsigned int uart_getc(void)
{    
#ifdef HAVE_UART_SYNC
    if (UCSR0A & _BV(RXC0)) {
        return ((UART0_STATUS & UART0_ERRMASK) << 8) | UART0_DATA;
    } else {
        return UART_NO_DATA;   /* no data available */
    }
#else
    unsigned char tmptail;
    unsigned char data;


    if ( UART_RxHead == UART_RxTail ) {
        return UART_NO_DATA;   /* no data available */
    }
    
    /* calculate /store buffer index */
    tmptail = (UART_RxTail + 1) & UART_RX_BUFFER_MASK;
    UART_RxTail = tmptail; 
    
    /* get data from receive buffer */
    data = UART_RxBuf[tmptail];
    
    return (UART_LastRxError << 8) + data;
#endif
}/* uart_getc */

#ifndef HAVE_UART_SYNC

#ifdef HAVE_UART_IRQ
SIGNAL(UART0_RECEIVE_INTERRUPT)
#else
static inline void poll_uart0_recv(void)
#endif
/*************************************************************************
Function: UART Receive Complete interrupt
Purpose:  called when the UART has received a character
**************************************************************************/
{
    unsigned char tmphead;
    unsigned char data;
    unsigned char usr;
    unsigned char lastRxError;

#ifndef HAVE_UART_IRQ
    if (!(UCSR0A & (1<<RXC0)))
        return;
    cli();
#endif
    DBG(0x38);
    usr  = UART0_STATUS;
    data = UART0_DATA;
    
    lastRxError = usr & UART0_ERRMASK;
    
    /* calculate buffer index */ 
    tmphead = ( UART_RxHead + 1) & UART_RX_BUFFER_MASK;
    
    if ( tmphead != UART_RxTail ) {
	UART_RxBuf[tmphead] = data;
	UART_RxHead = tmphead;
    } else {
	lastRxError = UART_BUFFER_OVERFLOW >> 8;
    }
    UART_LastRxError = lastRxError;   
    DBG(0x37);
#ifndef HAVE_UART_IRQ
    sei();
#endif
}


#ifdef HAVE_UART_IRQ
SIGNAL(UART0_TRANSMIT_INTERRUPT)
#else
static inline void poll_uart0_xmit(void)
#endif
/*************************************************************************
Function: UART Data Register Empty interrupt
Purpose:  called when the UART is ready to transmit the next byte
**************************************************************************/
{
    unsigned char tmptail;
    
#ifndef HAVE_UART_IRQ
    if (!(UCSR0A & (1<<UDRE0)))
        return;
    cli();
#endif
#ifdef DBGPORT
    uint8_t x = DBGPORT __attribute__((unused));
#endif
    DBG(0x36);
    if ( UART_TxHead != UART_TxTail) {
        /* calculate and store new buffer index */
        tmptail = (UART_TxTail + 1) & UART_TX_BUFFER_MASK;
        /* get one byte from buffer and write it to UART */
        UART0_DATA = UART_TxBuf[tmptail];  /* start transmission */
        UART_TxTail = tmptail;
    } else {
#ifdef HAVE_UART_IRQ
        /* tx buffer empty, disable UDRE interrupt */
        UART0_CONTROL &= ~_BV(UART0_UDRIE);
#endif
    }
    DBG(x);
#ifndef HAVE_UART_IRQ
    sei();
#endif
}

#endif /* sync */


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

    UART_TxHead = 0;
    UART_TxTail = 0;
    UART_RxHead = 0;
    UART_RxTail = 0;
    
#if defined( AT90_UART )
    /* set baud rate */
    UBRR = (unsigned char)baudrate; 

    /* enable UART receiver and transmmitter and receive complete interrupt */
    UART0_CONTROL = _BV(RXEN)|BV(TXEN);

#elif defined (ATMEGA_USART)
    /* Set baud rate */
    UBRRH = (unsigned char)(baudrate>>8);
    UBRRL = (unsigned char) baudrate;

    /* Enable USART receiver and transmitter and receive complete interrupt */
    UART0_CONTROL = (1<<RXEN)|(1<<TXEN);
    
    /* Set frame format: asynchronous, 8data, no parity, 1stop bit */
    #ifdef URSEL
    UCSRC = (1<<URSEL)|(3<<UCSZ0);
    #else
    UCSRC = (3<<UCSZ0);
    #endif 
    
#elif defined (ATMEGA_USART0 )
    /* Set baud rate */
    UBRR0H = (unsigned char)(baudrate>>8);
    UBRR0L = (unsigned char) baudrate;

    /* Enable USART receiver and transmitter and receive complete interrupt */
    UART0_CONTROL = (1<<RXEN0)|(1<<TXEN0);
    
    /* Set frame format: asynchronous, 8data, no parity, 2stop bit */
    #ifdef URSEL0
    UCSR0C = (1<<URSEL0)|(3<<UCSZ00)|_BV(USBS0);
    #else
    UCSR0C = (3<<UCSZ00)|_BV(USBS0);
    #endif 

#elif defined ( ATMEGA_UART )
    /* set baud rate */
    UBRRHI = (unsigned char)(baudrate>>8);
    UBRR   = (unsigned char) baudrate;

    /* Enable UART receiver and transmitter and receive complete interrupt */
    UART0_CONTROL = (1<<RXEN)|(1<<TXEN);

#else
#error I do not know your UART
#endif
#ifdef HAVE_UART_IRQ
#ifndef RXCIE
#define RXCIE RXCIE0
#endif
    UART0_CONTROL |= _BV(RXCIE);
#endif

}/* uart_init */


/*************************************************************************
Function: uart_putc()
Purpose:  write byte to ringbuffer for transmitting via UART
Input:    byte to be transmitted
Returns:  none          
**************************************************************************/
void uart_putc(unsigned char data)
{
    if(data == '\n')
        uart_putc('\r');

#ifdef HAVE_UART_SYNC
    while(!(UCSR0A & _BV(UDRE0))) ;
    UDR0 = data;
#else
    unsigned char sreg = SREG;
    cli();
#if 0 // shortcut
    if((UCSR0A & _BV(UDRE0)) && (UART_TxHead == UART_TxTail)) {
        UDR0 = data;
        SREG = sreg;
        return;
    }
#endif
    unsigned char tmphead = (UART_TxHead + 1) & UART_TX_BUFFER_MASK;

    if (tmphead != UART_TxTail) {
        UART_TxBuf[tmphead] = data;
        UART_TxHead = tmphead;

        /* enable UDRE interrupt */
#ifdef HAVE_UART_IRQ
        UART0_CONTROL    |= _BV(UART0_UDRIE);
#endif
    }

    SREG = sreg;
#endif /* sync */
}/* uart_putc */


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
void uart_puthex_word(const uint16_t b)
{
    if (b&0xFF00) {
        uart_puthex_byte(b>>8);
        uart_puthex_byte_(b);
    } else {
        uart_puthex_byte(b);
    }
} /* uart_puthex_word */

#if !defined(HAVE_UART_IRQ) && !defined(HAVE_UART_SYNC)
void uart_poll(void) {
    DBG(0x3E);
    poll_uart0_recv();
    DBG(0x3D);
    poll_uart0_xmit();
    DBG(0x3C);
}
#endif

#endif // HAVE_UART
