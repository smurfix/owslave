#ifndef MOAT_H
#define MOAT_H

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

#include "jmp.h"
#include "features.h"

/* Setup */
/* Called before enabling interrupts */
#ifndef init_state
void init_state(void);
#endif

/* Your main loop. You need to poll_all() if you don't return. */
void mainloop(void);

EXTERN q_jmp_buf _go_out;
static inline void go_out(void) __attribute__((noreturn));
static inline void go_out(void) {
    longjmp_q(_go_out);
}

/* Called to process commands. You implement this! */
void do_command(uint8_t cmd);
/*
   Your code can do any one of:
   * call xmit|recv_bit|byte, as required
   * call next_command() (wait for next bus command, will not return)
   * call next_idle() (wait for RESET pulse; will not return)

   If you need to run any expensive computations, do it in update_idle().
   Your steps need to be short enough to observe the timing requirements
   of the state you're currently in.

   Returning is equivalent to calling next_idle().
 */

/* Ditto, but called from idle / bit-wait context. You implement this! */
/* 'bits' says how many 1wire bit times are left. */
#ifndef update_idle
void update_idle(uint8_t bits);
#endif

void moat_init(void);
void moat_poll(void);

/* Implement if you need it. */
#ifdef CONDITIONAL_SEARCH
uint8_t condition_met(void);
#endif

#endif // moat.h
