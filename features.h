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

/* these are the current project settings, should be configurable in a way */

/* HAVE_UART is really critical, the POLLED_TRANSMITTER works better
 *   as it wont delay the 1-wire interrupts. This is especially true if large
 *   amounts of data are output (pin debug and edge debug on!)
 */
#define NHAVE_UART
#define NPOLLED_TRANSMITTER
#define BAUDRATE 57600
#define F_CPU 16000000		// cortex-M0 currently at 48000000
// #define SKIP_SEARCH		// omits search rom code (single slave only!)

/* some basic typedef, that should be very portable */
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned long u_long;


/*! there are 3 types of configuration settings:
 *  - uP specific settings (like uP type, register names etc.)
 *  - hardware specific settings (like fuse stuff: F_CPU, pin usage ...)
 *  - project specific settings (ds2408, ds2423, ...)
 */

#endif /* FEATURES_H */
