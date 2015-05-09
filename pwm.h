#ifndef PWM_H
#define PWM_H

#include "dev_data.h"
#include "features.h"
#include "timer.h"

#if defined(N_PWM)

typedef struct {
	uint8_t port;
	timer_t timer;
	uint16_t t_on,t_off;
	char is_on;
} pwm_t;

extern pwm_t pwms[];

void pwm_init(void);
void pwm_poll(void);

#else // no i/o

#define pwm_init() do {} while(0)
#define pwm_poll() do {} while(0)

#endif // any inputs or outputs at all
#endif // pwm_h
