#include <avr/io.h>
#include "vbus.h"



void vbus_receive( uint8_t c )
{
   if( c == VBUS_SYNC_CHAR )
   {
      vbus_state = VBUS_SYNC;
      vbus_crc = 0;
      vbus_out_buffer_ptr = 0;
      return;
   }
   if( vbus_state == VBUS_IDLE )
      return;
   /* Watch out for buffer overflow */
   if ( vbus_out_buffer_ptr >= VBUS_OUTBUFFERSIZE )
   {
      vbus_state = VBUS_IDLE;
      vbus_out_buffer_ptr = 0;
   }
   //FIXME this limits maximum of 16 packets (=16*4 = 64 bytes of data).
   switch(vbus_state & 0xF0 )
   {
   /* Receive the Head 10 bytes inc. Start-byte*/
   case VBUS_SYNC:
      // CRC Received
      if( vbus_state == 0x18 )
      {
         //vbus_out_buffer[vbus_state-0x10] = c;
         //vbus_state++;
         //vbus_out_buffer[vbus_state-0x10] = ~vbus_crc;
         if( ( ~vbus_crc & 0xFF ) == c )
            vbus_state = VBUS_DATA;
         else
            vbus_state = VBUS_IDLE;
         vbus_crc = 0;
         break; 
      }
      if( vbus_state < 0x18 )
      {
         /* Filtere commando */
         if( vbus_state == 0x10 && c != 0x10 )
         {
            vbus_state = VBUS_IDLE;
            return;
         }
         /* Anzahl Data Frames FIXME Also in out_buffer?*/
         if( vbus_state == 0x17 )
            vbus_framecount = c;
         switch ( vbus_state )
         {
            /* Looking for command 0x1000 here but it seems byte 2 is always 00 */
            case 0x12:
            case 0x13:
            case 0x17:
               vbus_out_buffer[vbus_out_buffer_ptr++] = c;
         }
         vbus_state++;
         vbus_crc += c;
      }
      break;
   case VBUS_DATA:
      /* Data Frame CRC */
      if( vbus_state == 0x25 )
      {
         if( ((( ~vbus_crc |0x80) - 0x80 )&0xFF) == c )
         {
            int copyloop;
            for( copyloop = 0; copyloop < 4; copyloop++ )
               vbus_out_buffer[vbus_out_buffer_ptr++] = vbus_in_buffer[copyloop];
            
         }
         else
            vbus_out_buffer[11] = 0x10;
         if( ++vbus_framepointer == vbus_framecount )
         {
            vbus_state = VBUS_IDLE;
            vbus_framecount = 0; 
         }
         vbus_state = VBUS_DATA;
         vbus_crc = 0;
         break;
      }
      /* Data Frame Septet */
     if( vbus_state == 0x24 )
      {
         vbus_crc+= c;
         vbus_in_buffer[0] = vbus_in_buffer[0] + (( c & 0x01) * 0x80 );
         vbus_in_buffer[1] = vbus_in_buffer[1] + (( c >> 1 & 0x01) * 0x80 );
         vbus_in_buffer[2] = vbus_in_buffer[2] + (( c >> 2 & 0x01) * 0x80 );
         vbus_in_buffer[3] = vbus_in_buffer[3] + (( c >> 3 & 0x01) * 0x80 );
      }
      /* 4 bytes of data */
      if( vbus_state < 0x24 )
      {
         vbus_crc+= c;
         vbus_in_buffer[vbus_state-0x20] = c;
      } 
      vbus_state++;
      break;
   default:
      vbus_state == VBUS_IDLE;
   }

}
