#ifndef UART_H
#define UART_H
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

#ifdef __AVR__
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
#define UART_BAUD_SELECT(baudRate,xtalCpu) ((xtalCpu)/((baudRate)*16l)-1)

#ifndef P
#define P(s) ({static const char c[] __attribute__ ((progmem)) = s;c;})
#endif

/**
 * @brief    Macro to automatically put a string constant into program memory
 */
#define uart_puts_P(__s)		uart_puts_p(P(__s))
#else		// __AVR__
// other uP usually don't need to distinguish memory spaces
#define uart_puts_P(__s)		uart_puts(__s)
#endif

/* 
** high byte error return code of uart_getc()
*/
#define UART_FRAME_ERROR      0x0800              /* Framing Error by UART       */
#define UART_OVERRUN_ERROR    0x0400              /* Overrun condition by UART   */
#define UART_BUFFER_OVERFLOW  0x0200              /* receive ring buffer overflow */
#define UART_NO_DATA          0x0100              /* no receive data available   */


/*
** function prototypes
*/

/**
   @brief   Initialize UART and set baudrate 
   @param   baudrate (implmentation specific)
   @return  none
*/
extern void uart_init(unsigned int baudrate);

/* only if polled mode enabled ! */
extern void uart_try_send(void);

/**
 *  @brief   Get received byte from ring buffer
 *
 * Returns in the lower byte the received character and in the 
 * higher byte the last receive error.
 * UART_NO_DATA is returned when no data is available.
 *
 *  @param   void
 *  @return  lower byte:  received byte from ring buffer
 *  @return  higher byte: last receive status
 *           - \b 0 successfully received data from UART
 *           - \b UART_NO_DATA           
 *             <br>no receive data available
 *           - \b UART_BUFFER_OVERFLOW   
 *             <br>Receive ring buffer overflow.
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
 *  @brief   Put byte to ring buffer for transmitting via UART
 */
extern void uart_putc(unsigned char data);
extern void uart_putc_now(unsigned char data);


/**
 *  @brief   Put string to ring buffer for transmitting via UART
 */
extern void uart_puts(const char *s );


/**
 * @brief    Put string from program memory to ring buffer for transmitting via UART.
 * @param    s program memory string to be transmitted
 */
extern void uart_puts_p(const char *s );

/**
 * @brief    Put integer to ring buffer for transmitting via UART.
 */
extern void uart_puti( int i );
extern void uart_putl( long i );

/**
 * @brief    Put lower nibble as hex to ring buffer for transmit via UART.
 */
extern void uart_puthex_nibble(unsigned char b);

/**
 * @brief    Put byte as hex to ring buffer for transmit via UART.
 */
extern void uart_puthex_byte(unsigned char b);

/**
 * @brief    Put two bytes as hex to ring buffer for transmit via UART.
 */
extern void uart_puthex_word(unsigned short b);

/**
 * @brief    Put 4 bytes as hex to ring-buffer for transmit via UART.
 */
extern void uart_puthex_long(unsigned long b);


/*@}*/
#endif // UART_H 

