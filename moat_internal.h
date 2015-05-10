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

typedef uint8_t read_len_fn(uint8_t chan);
typedef void read_fn(uint8_t chan, uint8_t *buf);
typedef void read_done_fn(uint8_t chan);
typedef char alert_check_fn(void);
typedef void alert_fill_fn(uint8_t *buf);
typedef void write_fn(uint16_t crc);

#define RDEFS(_s) \
    read_len_fn read_ ## _s ## _len; \
    read_fn read_ ## _s; \
    read_done_fn read_ ## _s ## _done;

#define WDEFS(_s) \
    write_fn write_ ## _s; 

#ifdef CONDITIONAL_SEARCH
#define ADEFS(_s) \
    alert_check_fn alert_ ## _s ## _check; \
    alert_fill_fn alert_ ## _s ## _fill;
#else
#define ADEFS(_s) // nothing
#endif

typedef struct {
    read_len_fn *read_len;
    read_fn *read;
    read_done_fn *read_done;
    write_fn *write;
#ifdef CONDITIONAL_SEARCH
    alert_check_fn *alert_check;
    alert_fill_fn *alert_fill;
#endif
} moat_call_t;
extern const moat_call_t moat_calls[TC_MAX] __attribute__ ((progmem));

extern const uint8_t moat_sizes[TC_MAX] __attribute__ ((progmem));

#define TC_DEFINE(x) \
    ADEFS(x) \
    RDEFS(x) \
    WDEFS(x)
#include "_def.h"
#undef TC_DEFINE

#endif // moat_internal.h
