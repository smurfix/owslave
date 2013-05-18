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

#define MAIN_CONTROL (0x01<<PA0) 
#define AUX_CONTROL  (0x01<<PA1)
#define MAIN_IN	(0x01<<PA3)
#define AUX_IN  (0x01<<PA2)
#define DEBUGP  (0x01<<PA5)

#define S_MODE       0x80
#define S_CONTROL    0x40
#define S_AUX_EVENT  0x20
#define S_MAIN_EVENT 0x10
#define S_AUX_LEVL   0x08
#define S_AUX_STAT   0x04
#define S_MAIN_LEVL  0x02
#define S_MAIN_STAT  0x01

volatile u_char status_info     = 0x00;

void do_command(u_char cmd)
{
   switch(cmd)
   {
      case C_CONTROL:
      {
         u_char control_byte=0;
         //DISCHARGE OFF
         DDRA &= ~(MAIN_IN|AUX_IN);
         //Update Status

         recv_byte();
         control_byte = recv_byte_in();
         //Write Low Enabled
         if( !(control_byte & 0x18) )
         {
             //Set Mode Auto or Manual.
             if ( control_byte & 0x20 )
                status_info |= S_MODE;
             else
                status_info &= ~S_MODE;
             status_info |= (control_byte & 0x20)<<3;
             //Set Control or Transistor if Mode Auto or Manual
             if( status_info&S_MODE) 
             {
                //Manual Mode
                //FIXME Set output transistor.
                if ( control_byte & 0x80 )
                   status_info |= S_CONTROL;
                else
                   status_info &= ~S_CONTROL;
             }
             else
             {
                //Auto Mode
                //FIXME Set output transistor.
                if ( control_byte & 0x40 )
                   status_info |= S_CONTROL;
                else
                   status_info &= ~S_CONTROL;
             }
         }
         //Check if Main and Aux have no short circuit
         if( PINA & MAIN_IN )
            status_info |= S_MAIN_LEVL;
         else
            status_info &= ~S_MAIN_LEVL;

         if( PINA & AUX_IN )
            status_info |= S_AUX_LEVL;
         else
            status_info &= ~S_AUX_LEVL;

         xmit_byte(status_info);
         xmit_byte(status_info);
         break;
      }
      case C_ALL_OFF:
      {
         //DISCHARGE OFF
         DDRA &= ~(MAIN_IN|AUX_IN);
         //DEACTIVATE MAIN and AUX
         PORTA |= (MAIN_CONTROL|AUX_CONTROL);
         //Clear Event Flags
         status_info &= ~(S_MAIN_EVENT|S_AUX_EVENT); 
         //UPDATE AUTO CONTROL
         if( !(status_info&S_MODE) )
            status_info |= (S_MAIN_STAT|S_AUX_STAT);
         //Return Acknowledge
         xmit_byte(C_ALL_OFF);
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
         status_info &= ~(S_MAIN_STAT);
         if( !(status_info&S_MODE) )
         {
            status_info &= ~(S_CONTROL);   
         }
         //FIXME Set output transistor.
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
            xmit_bit_out();
         }
         if( shortcircuit )
         {
            //Return Inverted Command
            xmit_byte(~C_SMART_MAIN_ON);
            xmit_byte_out();
            //UPDATE AUTO CONTROL
            status_info &= ~S_MAIN_LEVL;
         }
         else
         {
            //Return Command
            xmit_byte(C_SMART_MAIN_ON);
            xmit_byte_out();
            //Add some delay.
            for ( loop = 300; loop > 0; loop -- )
            {
               presence&=(PINA&MAIN_IN);
            }
            //AUX OFF
            PORTA |= (AUX_CONTROL);
            //MAIN ON
            PORTA &= ~(MAIN_CONTROL);
            //set_idle();
            //UPDATE AUTO CONTROL
            status_info |= S_AUX_STAT;
            status_info &= ~S_MAIN_STAT;
            if( !(status_info&S_MODE) )
               status_info &= ~(S_CONTROL);
            //FIXME Set output transistor.
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
         for(loop=3;loop>0;loop--)
         {
            shortcircuit &= (PINA&AUX_IN)?0:1;
         }
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
            xmit_bit_out();
         }
         if( shortcircuit )
         {
            //Return Inverted Command
            xmit_byte(~C_SMART_AUX_ON);
            xmit_byte_out();
            //UPDATE AUTO CONTROL
            status_info &= ~S_AUX_LEVL;
         }
         else
         {
            //Return Command
            xmit_byte(C_SMART_AUX_ON);
            xmit_byte_out();
            //Add some delay.
            for ( loop = 300; loop > 0; loop -- )
            {
               presence&=(PINA&AUX_IN);
            }
            //MAIN OFF
            PORTA |= (MAIN_CONTROL);
            //AUX ON
            PORTA &= ~(AUX_CONTROL);
            //UPDATE AUTO CONTROL
            status_info |= S_MAIN_STAT;
            status_info &= ~S_AUX_STAT;
            if( !(status_info&S_MODE) )
               status_info |= S_CONTROL;
            //FIXME Set output transistor.
         }
         break;
      }
      default:
         set_idle();
   }
}

void update_idle(u_char bits)
{
        //Check for events
        //FIXME maybe pinchange interrupts
        //Check if Main inactive
        if( status_info & S_MAIN_STAT )
           //Pin Low is Event
           if( ! (PINA&MAIN_IN))
              status_info |= S_MAIN_EVENT;
        //Check if Aux inactive
        if( status_info & S_AUX_STAT )
           //Pin Low is Event
           if( ! (PINA&AUX_IN))
              status_info |= S_AUX_EVENT;
}

void init_state(void)
{
   DDRA |= (MAIN_CONTROL|AUX_CONTROL);
   //PORTA &= ~(MAIN_CONTROL);
   //Set MAIN Control, AUX Control and Presence Outputs
   DDRA |= (MAIN_CONTROL|AUX_CONTROL);
   //DDRA |= (DEBUGP);
   //Switch Off MAIN, AUX and Presence
   PORTA |= (MAIN_CONTROL|AUX_CONTROL);
   //PORTA &= ~(DEBUGP);
   //Set MAIN In and Aux HIZ inputs, low output
   DDRA &= ~(MAIN_IN|AUX_IN);
   PORTA &= ~(MAIN_IN|AUX_IN);
}

