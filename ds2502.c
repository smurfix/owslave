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

/* This code implements (some of) the DS2408 8-bit I/O (obsolete).
 */

#include "onewire.h"
#include "vbus.h" 

#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>

#define C_READ_DATA      0xC3 
#define C_STATUS      0xAA 

#define UART0_RECEIVE_INTERRUPT   USART_RX_vect
#define UART0_TRANSMIT_INTERRUPT  USART_UDRE_vect
#define UART0_STATUS   UCSRA
#define UART0_CONTROL  UCSRB
#define UART0_DATA     UDR
#define UART0_UDRIE    UDRIE
#define BAUD 9600
#define BAUDRATE (F_CPU / 16 / BAUD )-1

extern volatile uint8_t vbus_out_buffer[32];

void do_read(void)
{
	uint8_t crc = 0;
	uint16_t adr;
	uint16_t bcrc = 1;
	uint8_t b;

	recv_byte();
	crc = crc8(crc,0xC3); //FIXME, maybe predefine?
	b = recv_byte_in();
	recv_byte();
	crc = crc8(crc,b);
	adr = b;
	b = recv_byte_in();
	crc = crc8(crc,b);
	adr |= b<<8;
	xmit_byte(crc);
        crc = 0;
#define XMIT(val) do {                                     \
		xmit_byte(val);                            \
		crc = crc8(crc,val);                      \
	} while(0)
        
	while(1)
        {
           if( adr < 0x20 )
              XMIT(vbus_out_buffer[adr]);
           else
	      XMIT(0xFF);
           adr++;
           if((adr%0x20) == 0 )
           {
              xmit_byte(crc);
              crc = 0;
           }
        }
        while(1)
           xmit_bit(1);
}

void do_command(uint8_t cmd)
{
        switch(cmd)
        {
           case C_READ_DATA: 
              do_read();
              break;
           default:           
		DBG_P("?CI");
		DBG_X(cmd);
		set_idle();
        }
}

void update_idle(uint8_t bits)
{
}

void init_state(void)
{
//Init UART for VBUS operation this is not for debugging.
    UBRRH = (unsigned char)(BAUDRATE>>8);
    UBRRL = (unsigned char) BAUDRATE;
    //UART0_CONTROL = _BV(RXCIE)|(1<<RXEN)|(1<<TXEN);
    UART0_CONTROL = _BV(RXCIE)|(1<<RXEN);
    // Set frame format: asynchronous, 8data, no parity, 1stop bit 
    UCSRC = (3<<UCSZ0);
}

SIGNAL(UART0_RECEIVE_INTERRUPT)
{
   unsigned char data;
   data = UART0_DATA;
   vbus_receive(data);
}

