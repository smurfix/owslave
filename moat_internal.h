#ifndef MOAT_I_H
#define MOAT_I_H

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

#include "moat.h"

#define MAXBUF 32

/* transmit the CRC, check that its complement is received correctly.
   If it is not, this will not return to the caller.
   */
void end_transmission(uint16_t crc);

#define _NONE ({next_idle(':'); 0})
#ifdef N_CONSOLE
uint8_t read_console_len(uint8_t chan);
void read_console(uint8_t chan, uint8_t *buf);
void read_console_done(uint8_t chan);
#else
#define read_console(c,b) _NONE
#define read_console_done(crc) do{}while(0)
#endif

#ifdef N_PORT
uint8_t read_port_len(uint8_t chan);
void read_port(uint8_t chan, uint8_t *buf);
void read_port_done(uint8_t chan);
void write_port(uint16_t crc);
#else
#define read_port(c,b) _NONE
#define read_port_done(crc) do{}while(0)
#define write_port(crc) do{}while(0)
#endif

#ifdef N_PWM
uint8_t read_pwm_len(uint8_t chan);
void read_pwm(uint8_t chan, uint8_t *buf);
void write_pwm(uint16_t crc);
#else
#define read_pwm(c,b) _NONE
#define write_pwm(crc) do{}while(0)
#endif

#ifdef N_COUNT
uint8_t read_count_len(uint8_t chan);
void read_count(uint8_t chan, uint8_t *buf);
void write_count(uint16_t crc);
#else
#define read_count(c,b) _NONE
#define write_count(crc) do{}while(0)
#endif

#if defined(N_CONSOLE) && defined(CONSOLE_WRITE)
void write_console(uint16_t crc);
#else
#define write_console(crc) do{}while(0)
#endif

#endif // moat_internal.h
