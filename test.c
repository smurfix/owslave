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

/* This code implements a smple test code which uses a delay loop to toggle
 * a pin and emit a serial character.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include "onewire.h"
#include "features.h"
#include "debug.h"

static unsigned long long x = 0;
void setup(void) {
	DBG_OUT();
	DBG_OFF();
}

void mainloop(void) {
	if(++x<100000ULL) return;
	x = 0;
	DBG_ON();
	DBG_OFF();
}
