#ifndef COUNT_H
#define COUNT_H

#include "dev_data.h"
#include "features.h"

#if defined(N_COUNT)

typedef struct {
	uint8_t port;
	unsigned char flags;
#define CF_FLANK_MASK 0x6
#define CF_ALERTING (1<<0)
#define CF_FALLING_ONLY (1<<1)
#define CF_RISING_ONLY (1<<2)
#define CF_IS_ALERT (1<<6)
#define CF_IS_ON (1<<7)
	uint16_t count;
#define COUNT_SIZE 2
} count_t;

extern count_t counts[];

#ifdef CONDITIONAL_SEARCH
extern uint8_t count_changed_cache;
#endif

#endif // any inputs or outputs at all
#endif // count_h
