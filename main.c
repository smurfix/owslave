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

/* Based on work published at http://www.mikrocontroller.net/topic/44100 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <setjmp.h>

#define MAIN
#include "features.h"
#include "onewire.h"
#include "uart.h"
#include "dev_data.h"
#include "debug.h"

// Initialise the hardware
static inline void
setup(void)
{
	mcu_init();
	uart_init(UART_BAUD_SELECT(BAUDRATE,F_CPU));
	onewire_init();
}

// Main program
int
main(void)
{
#ifdef DBGPIN
	OWPORT &= ~(1 << DBGPIN);
	OWDDR |= (1 << DBGPIN);
#endif
	OWDDR &= ~(1<<ONEWIREPIN);
	OWPORT &= ~(1<<ONEWIREPIN);

	DBG_IN();

#ifdef HAVE_TIMESTAMP
	tbpos = sizeof(tsbuf)/sizeof(tsbuf[0]);
	uint16_t last_tb = 0;
#endif

	setup();

	set_idle();

	// now go
	sei();
	DBGS_P("\nInit done!\n");
	while(1) mainloop();
}

