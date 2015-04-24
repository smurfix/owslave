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

/* transmit the CRC, check that its complement is received correctly.
   If it is not, this will not return to the caller.
   */
void end_transmission(uint16_t crc);

#ifdef TC_CONSOLE
void read_console(uint16_t crc);
#else
#define read_console(crc) do{}while(0)
#endif

#ifdef TC_PORT
void read_port(uint16_t crc);
void write_console(uint16_t crc);
#else
#define read_port(crc) do{}while(0)
#define write_port(crc) do{}while(0)
#endif

#if defined(TC_CONSOLE) && defined(CONSOLE_WRITE)
void write_console(uint16_t crc);
#else
#define write_console(crc) do{}while(0)
#endif

#endif // moat_internal.h
