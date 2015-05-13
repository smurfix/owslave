#ifndef TIMER_H
#define TIMER_H

#include "dev_data.h"
#include "features.h"

#ifdef HAVE_TIMER

typedef struct {
	int16_t last;
} timer_t;

/* return True every sec tenth seconds */
char timer_done(timer_t *t);
void timer_start(int16_t sec, timer_t *t);
int16_t timer_remaining(timer_t *t);

void timer_reset(timer_t *t);

int16_t timer_counter(void);

/**
 * The point of 'reset_delta' is: assume every(20,&t) controls a 50% PWM output
 * and is called half a second too late at the end of the 'off' phase. You have 
 * three choices:
 * = do nothing: the 'on' phase will last 1.5 seconds, thus the next on>off
 *   transition isn't delayed.
 * = call reset(): 'on' will be 2.0 seconds, thus keeping individual timing
 *   as accurately aspossible
 * = call reset_delta(20,&t): 'on' will be 2.5 seconds, thereby making sure
 *   that the on/off ratio isn't disturbed too much.
 */

void timer_init(void);
void timer_poll(void);

#else // no timer

#define timer_init() do {} while(0)
#define timer_poll() do {} while(0)

#endif // !HAVE_TIMER

#endif // timer_h
