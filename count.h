#ifndef COUNT_H
#define COUNT_H

#include "dev_data.h"
#include "features.h"
#include "timer.h"

#if defined(N_COUNT)

#define CF_ALERTING 0x01
#define CF_IS_ALERT 0x02
#define CF_IS_ON 0x80

typedef struct {
	uint8_t port;
	unsigned char flags;
	uint16_t count;
} count_t;

extern count_t counts[];

EXTERN uint8_t count_changed_cache;

#ifdef CONDITIONAL_SEARCH
static inline char count_alert(void) {
        if (count_changed_cache)
                return 1;
        return 0;
}

#else
#define count_alert() 0
#endif

void count_init(void);
void count_poll(void);

#else // no i/o

#define count_init() do {} while(0)
#define count_poll() do {} while(0)
#define count_alert() 0

#endif // any inputs or outputs at all
#endif // count_h
