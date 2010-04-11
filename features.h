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

#ifndef FEATURES_H
#define FEATURES_H

/* External definitions:
 * F_CPU       clock rate
 * SKIP_SEARCH if you don't have enough ROM for search (saves ~200 bytes)
 * HAVE_UART   if you want to debug your code
 */
#ifdef __AVR_ATtiny13__
#define F_CPU_                9600000
#define SKIP_SEARCH
#endif

#ifdef __AVR_ATmega8__
#define F_CPU_                8000000
#endif

#ifdef __AVR_ATmega168__
#define F_CPU_                16000000
// #define HAVE_UART
#endif

#ifndef F_CPU_
#error Unknown AVR chip!
#endif

#ifndef F_CPU
#define F_CPU F_CPU_
#endif

#endif /* FEATURES_H */
