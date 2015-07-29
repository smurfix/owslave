#ifndef UART_H
#define UART_H
#include "features.h"

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

/* This code is based on work by: */

/************************************************************************
Title:     Interrupt UART library with receive/transmit circular buffers
Author:    Peter Fleury <pfleury@gmx.ch>   http://jump.to/fleury
File:      not valid see "Extension" 
           ($Id: uart.h,v 1.7.2.1 2003/12/27 20:39:14 peter Exp $)
Software:  AVR-GCC 3.3/3.4
Hardware:  any AVR with built-in UART, tested on AT90S8515 at 4 Mhz and
           ATmega16 at 1.8, 3.6, 4 and 8 MHz
Usage:     see Doxygen manual
Extension: uart_puti, uart_puthex_nibble, uart_puthex_byte 
           by M.Thomas 9/2004
************************************************************************/

/** 
 *  @defgroup pfleury_uart UART Library
 *  @code #include <uart.h> @endcode
 * 
 *  @brief Interrupt UART library using the built-in UART with transmit and receive circular buffers. 
 *
 *  This library can be used to transmit and receive data through the built in UART. 
 *
 *  An interrupt is generated when the UART has finished transmitting or
 *  receiving a byte. The interrupt handling routines use circular buffers
 *  for buffering received and transmitted data.
 *
 *  The UART_RX_BUFFER_SIZE and UART_TX_BUFFER_SIZE constants define
 *  the size of the circular buffers in bytes. Note that these constants must be a power of 2.
 *  You may need to adapt this size to your target and your application.
 *
 *  @note Based on Atmel Application Note AVR306
 *  @author Peter Fleury pfleury@gmx.ch  http://jump.to/fleury
 */
 
/*@{*/

#if (__GNUC__ * 100 + __GNUC_MINOR__) < 303
#error "This library requires AVR-GCC 3.3 or later, update to newer AVR-GCC compiler !"
#endif


/*
** constants and macros
*/

/** @brief  UART Baudrate Expression
 *  @param  xtalcpu  system clock in Mhz           
 *  @param  baudrate baudrate in bps, e.g. 1200, 2400, 9600     
 */
//#define UART_BAUD_SELECT(baudRate,xtalCpu) ((xtalCpu)/((baudRate)*16l)-1)
#define UART_BAUD_SELECT(baudRate,xtalCpu) ((xtalCpu + baudRate * 8L) / (baudRate * 16L) - 1)


#ifndef P
#define P(s) ({static const char c[] __attribute__ ((progmem)) = s;c;})
#endif


/* 
** high byte error return code of uart_getc()
*/
#define UART_FRAME_ERROR      0x0800              /* Framing Error by UART       */
#define UART_OVERRUN_ERROR    0x0400              /* Overrun condition by UART   */
#define UART_BUFFER_OVERFLOW  0x0200              /* receive ringbuffer overflow */
#define UART_NO_DATA          0x0100              /* no receive data available   */

#ifdef HAVE_UART

/*
** function prototypes
*/

/**
   @brief   Initialize UART and set baudrate 
   @param   baudrate Specify baudrate using macro UART_BAUD_SELECT()
   @return  none
*/
extern void uart_init(unsigned int baudrate);


/**
 *  @brief   Get received byte from ringbuffer
 *
 * Returns in the lower byte the received character and in the 
 * higher byte the last receive error.
 * UART_NO_DATA is returned when no data is available.
 *
 *  @param   void
 *  @return  lower byte:  received byte from ringbuffer
 *  @return  higher byte: last receive status
 *           - \b 0 successfully received data from UART
 *           - \b UART_NO_DATA           
 *             <br>no receive data available
 *           - \b UART_BUFFER_OVERFLOW   
 *             <br>Receive ringbuffer overflow.
 *             We are not reading the receive buffer fast enough, 
 *             one or more received character have been dropped 
 *           - \b UART_OVERRUN_ERROR     
 *             <br>Overrun condition by UART.
 *             A character already present in the UART UDR register was 
 *             not read by the interrupt handler before the next character arrived,
 *             one or more received characters have been dropped.
 *           - \b UART_FRAME_ERROR       
 *             <br>Framing Error by UART
 */
extern unsigned int uart_getc(void);


/**
 *  @brief   Put byte to ringbuffer for transmitting via UART
 *  @param   data byte to be transmitted
 *  @return  none
 */
extern void uart_putc(unsigned char data);


/**
 *  @brief   Put string to ringbuffer for transmitting via UART
 *
 *  The string is buffered by the uart library in a circular buffer
 *  and one character at a time is transmitted to the UART using interrupts.
 *  Blocks if it can not write the whole string into the circular buffer.
 * 
 *  @param   s string to be transmitted
 *  @return  none
 */
extern void uart_puts(const char *s );


/**
 * @brief    Put string from program memory to ringbuffer for transmitting via UART.
 *
 * The string is buffered by the uart library in a circular buffer
 * and one character at a time is transmitted to the UART using interrupts.
 * Blocks if it can not write the whole string into the circular buffer.
 *
 * @param    s program memory string to be transmitted
 * @return   none
 * @see      uart_puts_P
 */
extern void uart_puts_p(const char *s );

/**
 * @brief    Macro to automatically put a string constant into program memory
 */
#define uart_puts_P(__s)       uart_puts_p(P(__s))

/**
 * @brief    Put integer to ringbuffer for transmitting via UART.
 *
 * The integer is converted to a string which is buffered by the uart 
 * library in a circular buffer and one character at a time is transmitted 
 * to the UART using interrupts.
 *
 * @param    value to transfer
 * @return   none
 * @see      uart_puts_p
 */
extern void uart_puti( int i );
extern void uart_putl( long i );

/**
 * @brief    Put nibble as hex to ringbuffer for transmit via UART.
 *
 * The lower nibble of the parameter is convertet to correspondig
 * hex-char and put in a circular buffer and one character at a time 
 * is transmitted to the UART using interrupts.
 *
 * @param    value to transfer (byte, only lower nibble converted)
 * @return   none
 * @see      uart_putc
 */
extern void uart_puthex_nibble(const unsigned char b);

/**
 * @brief    Put byte as hex to ringbuffer for transmit via UART.
 *
 * The upper and lower nibble of the parameter are convertet to 
 * correspondig hex-chars and put in a circular buffer and one 
 * character at a time is transmitted to the UART using interrupts.
 * The version with an additional underscore will always emit two
 * characters.
 *
 * @param    value to transfer
 * @return   none
 * @see      uart_puthex_nibble
 */
extern void uart_puthex_byte(const unsigned char b);
extern void uart_puthex_byte_(const unsigned char b);

/**
 * @brief    Put two bytes as hex to ringbuffer for transmit via UART.
 *
 * The upper and lower bytes of the parameter are convertet to 
 * correspondig hex-chars and put in a circular buffer and one 
 * character at a time is transmitted to the UART using interrupts.
 *
 * @param    value to transfer
 * @return   none
 * @see      uart_puthex_byte
 */
extern void uart_puthex_word(const uint16_t b);

/**
 * @brief    Poll the UART.
 *
 * Call this from your main loop.
 *
 * Does nothing if HAVE_UART_IRQ is defined.
 *
 * @return   none
 */
#if !defined(HAVE_UART_IRQ) && !defined(HAVE_UART_SYNC)
extern void uart_poll(void);
#else
#define uart_poll() do {} while(0)
#endif
/*@}*/

#else // !HAVE_UART

#define uart_init(baud) do {} while(0)
#define uart_getc() UART_NO_DATA
#define uart_putc(data) do {} while(0)
#define uart_puts(s) do {} while(0)
#define uart_puts_p(s) do {} while(0)
#define uart_puts_P(s) do {} while(0)
#define uart_puti(i) do {} while(0)
#define uart_putl(i) do {} while(0)
#define uart_puthex_nibble(b) do {} while(0)
#define uart_puthex_byte(b) do {} while(0)
#define uart_puthex_byte_(b) do {} while(0)
#define uart_puthex_word(b) do {} while(0)
#define uart_poll() do {} while(0)
/*@}*/

#endif // !HAVE_UART
#endif // UART_H 

