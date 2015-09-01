/*
 *  Copyright © 2014-2015, Matthias Urlichs <matthias@urlichs.de>
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

/* This code implements pulse width modulation.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include "pgm.h"
#include <string.h>

#include "features.h"

#ifdef N_STATUS
#define STATUS_no_extern
#include "status.h"

#include "moat.h"
#include "dev_data.h"
#include "debug.h"
#include "moat_internal.h"

t_status_boot status_boot = S_boot_unknown;
extern uint8_t mcusr;

#ifdef CONDITIONAL_SEARCH
uint8_t init_msg = 0x01;
#endif

void init_status(void)
{
	if (mcusr & S_boot_irq) {
		status_boot = mcusr;
	} else if (mcusr & (1<<WDRF))
		status_boot = S_boot_watchdog;
	else if (mcusr & (1<<BORF))
		status_boot = S_boot_brownout;
	else if (mcusr & (1<<EXTRF))
		status_boot = S_boot_external;
	else if (mcusr & (1<<PORF))
		status_boot = S_boot_powerup;
	else
		status_boot = S_boot_unknown;
}

#endif
