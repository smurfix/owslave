#ifndef TEMP_H
#define TEMP_H

#include "dev_data.h"
#include "features.h"

#if defined(N_TEMP)

#define TEMP_AGAIN (int16_t)0x8000

// values are °C << 5
// we could use K here, but we're not because most sensors return °C anyway
typedef struct {
	uint8_t flags;
	uint8_t device;
	int16_t value;
	int16_t lower;
	int16_t upper;
} temp_t;
extern temp_t temps[];

typedef void temp_init_fn(void);
typedef void temp_setup_fn(uint8_t devno);
typedef int16_t temp_poll_fn(uint8_t devno);

typedef struct {
    temp_init_fn *init;
    temp_setup_fn *setup;
    temp_poll_fn *poll;
} temp_call_t;

#define TDEFS(_s) \
    temp_init_fn temp_init_ ## _s; \
    temp_setup_fn temp_setup_ ## _s; \
    temp_poll_fn temp_poll_ ## _s; 

#define TFUNCPTRS(_s) \
{ \
    &temp_init_ ## _s, \
    &temp_setup_ ## _s, \
    &temp_poll_ ## _s, \
}

#define TEMP_TC_DEFINE(x) \
    TDEFS(x)
#include "_temp_defs.h"
#undef TEMP_TC_DEFINE

// the first two bits are used for temp_out_t, i.e. PO_* constants. Hardcoded.
#define TEMP_MASK       ((1<<5)-1) // drievr number
#define TEMP_ALERT      (1<<5)  // trigger an alarm when stepping over boundary
#define TEMP_IS_ALERT_L (1<<6)  // alarm triggered (low)?
#define TEMP_IS_ALERT_H (1<<7)  // alarm triggered (high)?

#ifdef CONDITIONAL_SEARCH

extern uint8_t temp_changed_cache;
static inline char temp_alert(void) {
	if (temp_changed_cache)
		return 1;
	return 0;
}

#else
#define temp_alert() 0
#endif

#else // no i/o

#define alert_temp() 0

#endif // any inputs or outputs at all
#endif // temp_h
