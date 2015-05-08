#ifndef TIMER_H
#define TIMER_H

#include "dev_data.h"
#include "features.h"

#if defined(HAVE_TIMER)

typedef struct {
	int16_t current;
} timer_t;

/* return True every S seconds */
char every(int16_t sec, timer_t *t);

void timer_init(void);
void timer_poll(void);

#endif

#else // no timer

#define timer_init() do {} while(0)
#define timer_poll() do {} while(0)
#define every(a,b) 0

#endif // timer_h
