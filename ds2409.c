/*
 *  Copyright © 2010, Matthias Urlichs <matthias@urlichs.de>
 *  Copyright (c) 2013, Marc Dirix <marc@dirix.nu>
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

/* This code implements (some of) the DS2409 MicroLan Coupler (obsolete).
 */
#include <avr/io.h>
#include "features.h"
#include "onewire.h"

#define C_CONTROL    0x5A
#define C_ALL_OFF    0x66
#define C_DISCHARGE  0x99
#define C_DIRECT_MAIN_ON 0xA5
#define C_SMART_MAIN_ON 0xCC
#define C_SMART_AUX_ON 0x33

#define MAIN_CONTROL 0x01<<PA0 
#define AUX_CONTROL  0x01<<PA1
#define MAIN_IN	0x01<<PA3
#define AUX_IN  0x01<<PA2
#define DEBUGP  0x01<<PA5

static u_char
	status_info     = 0x00;

void do_status()
{
   u_char control_byte=0;

   recv_byte();
   control_byte = recv_byte_in();
   //Write Low Enabled
   if( !(control_byte & 0x18) )
   {
      status_info |= (control_byte & 0x20)?0x80:0x00;
      if( status_info&0x80 )
         status_info |= (control_byte & 0x40)?0x40:0x00;
      else
         status_info |= (control_byte & 0x80)?0x40:0x00;
      
   }
   //Check if Main and Aux have no short circuit
   if( PINA & MAIN_IN )
      status_info |= 0x02;
   else
      status_info &= ~0x02;
   if( PINA & AUX_IN )
      status_info |= 0x04;
   else
      status_info &= ~0x04;
   status_info |= 0x20;
   status_info |= 0x10;
   xmit_byte(status_info);
   xmit_byte(status_info);
}

void do_command(u_char cmd)
{
   switch(cmd)
   {
      case C_CONTROL:
         //DISCHARGE OFF
         DDRA &= ~(MAIN_IN|AUX_IN);
         //Update Status
         do_status();
         break;
      case C_ALL_OFF:
      {
         //DISCHARGE OFF
         DDRA &= ~(MAIN_IN|AUX_IN);
         //DEACTIVATE MAIN and AUX
         PORTA |= (MAIN_CONTROL|AUX_CONTROL);
         //Return Acknowledge
         xmit_byte(C_ALL_OFF);
         //Clear Event Flags
         status_info &= 0x67; 
         //UPDATE AUTO CONTROL
         if( !(status_info&0x80) )
            status_info &= 0xBF;
         break;
      }
      case C_DISCHARGE:
         //DEACTIVATE MAIN and AUX
         PORTA |= (MAIN_CONTROL|AUX_CONTROL);
         //DISCHARGE ON
         DDRA |= (MAIN_IN|AUX_IN);
         xmit_byte(C_DISCHARGE);
         break;
      case C_DIRECT_MAIN_ON:
         //DISCHARGE OFF
         //DDRA &= ~(MAIN_IN|AUX_IN);
         //AUX OFF
         PORTA |= (AUX_CONTROL);
         //MAIN ON
         PORTA &= ~(MAIN_CONTROL);
         //UPDATE AUTO CONTROL
         xmit_byte(C_DIRECT_MAIN_ON);
         if( !(status_info&0x80) )
            status_info &= 0xBF;
         break;
      case C_SMART_MAIN_ON:
      {
         u_char shortcircuit=0,presence=0;
         u_short loop;
         //DISCHARGE OFF
         DDRA &= ~(MAIN_IN|AUX_IN);
         //MAIN OFF
         PORTA |= (MAIN_CONTROL);
         //Test Short Circuit
         shortcircuit = (PINA&MAIN_IN)?0:1;
         //RESET ON
         recv_bit();
         recv_bit_in();
         DDRA |= (MAIN_IN);
         //recv_byte();
         for(loop=7;loop>0;loop--)
         {
            recv_bit();
            recv_bit_in();
         }
         //RESET OFF
         DDRA &= ~(MAIN_IN);
         //Sleep 50us
         for( loop = 750; loop > 0; loop--)
         {
            presence &= (PINA&MAIN_IN)?1:0;
         }
         //PORTA &= ~DEBUGP;
         //Copy MAIN_IN to 1-wire
         for(loop=8;loop>0;loop--)
         {
            xmit_bit( presence );
            //xmit_bit( 1 );
            //xmit_bit_out();
         }
         if( shortcircuit )
         {
            //Return Inverted Command
            xmit_byte(~C_SMART_MAIN_ON);
            //UPDATE AUTO CONTROL
            status_info &= ~0x02;
         }
         else
         {
            //Return Command
            xmit_byte(C_SMART_MAIN_ON);
            //FIXME Wait for xmit
            //PORTA |= DEBUGP;
            for( loop=3200; loop > 0; loop--)
            {
               //Test shortcuit here?
               presence &=(PINA&MAIN_IN);
            }
            //PORTA &= DEBUGP;
            //AUX OFF
            PORTA |= (AUX_CONTROL);
            //MAIN ON
            PORTA &= ~(MAIN_CONTROL);
            //set_idle();
            //UPDATE AUTO CONTROL
            status_info |= 0x04;
            status_info &= ~0x01;
            if( !(status_info&0x80) )
               status_info &= 0xBF;
         }
         break;
      }
      case C_SMART_AUX_ON:
      {
         u_char shortcircuit=0,presence=0;
         u_short loop;
         //DISCHARGE OFF
         DDRA &= ~(MAIN_IN|AUX_IN);
         //AUX OFF
         PORTA |= (AUX_CONTROL);
         //Test Short Circuit
         shortcircuit = (PINA&AUX_IN)?0:1;
         //RESET ON
         recv_bit();
         recv_bit_in();
         DDRA |= (AUX_IN);
         //recv_byte();
         for(loop=7;loop>0;loop--)
         {
            recv_bit();
            recv_bit_in();
         }
         //RESET OFF
         DDRA &= ~(AUX_IN);
         //Check for presence puls during delay.
         for( loop = 750; loop > 0; loop--)
         {
            presence &= (PINA&AUX_IN)?1:0;
         }
         //PORTA &= ~DEBUGP;
         //Copy AUX_IN to 1-wire
         for(loop=8;loop>0;loop--)
         {
            xmit_bit( presence );
            //xmit_bit( 1 );
            //xmit_bit_out();
         }
         if( shortcircuit )
         {
            //Return Inverted Command
            xmit_byte(~C_SMART_AUX_ON);
            //UPDATE AUTO CONTROL
            status_info &= ~0x02;
         }
         else
         {
            //Return Command
            xmit_byte(C_SMART_AUX_ON);
            //FIXME Wait for xmit instead of looping
            //PORTA |= DEBUGP;
            for( loop=3200; loop > 0; loop--)
            {
               //Test shortcuit here?
               presence &=(PINA&AUX_IN);
            }
            //PORTA &= DEBUGP;
            //MAIN OFF
            PORTA |= (MAIN_CONTROL);
            //AUX ON
            PORTA &= ~(AUX_CONTROL);
            //set_idle();
            //UPDATE AUTO CONTROL
            /*status_info |= 0x04;
            status_info &= ~0x01;
            if( !(status_info&0x80) )
               status_info &= 0xBF;*/
         }
         break;
      }
      default:
         set_idle();
   }
}

void update_idle(u_char bits)
{
	//DBG_C('\\');
	//uart_try_send();
}

void init_state(void)
{
   DDRA |= (MAIN_CONTROL|AUX_CONTROL);
   //PORTA &= ~(MAIN_CONTROL);
   //Set MAIN Control, AUX Control and Presence Outputs
   DDRA |= (MAIN_CONTROL|AUX_CONTROL|DEBUGP);
   //Switch Off MAIN, AUX and Presence
   //PORTA |= (MAIN_CONTROL|AUX_CONTROL);
   PORTA |= (MAIN_CONTROL|AUX_CONTROL);
   PORTA &= ~(DEBUGP);
   //Set MAIN In and Aux HIZ inputs, low output
   DDRA &= ~(MAIN_IN|AUX_IN);
   PORTA &= ~(MAIN_IN|AUX_IN);
}

