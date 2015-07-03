#ifndef PWM_H
#define PWM_H

#include "dev_data.h"
#include "features.h"
#include "timer.h"

#if defined(N_PWM)

typedef struct {
	uint8_t port;
	uint8_t flags;
	timer_t timer;
	uint16_t t_on,t_off;
#define PWM_ALERT    (1<<0) // alert when one-shot PWM stops
#define PWM_FORCE    (1<<1) // switch immediately when setting PWM
#define PWM_IS_ALERT (1<<6) // alert present
#define PWM_IS_ON    (1<<7) // PWM is in OM phase
} pwm_t;

extern pwm_t pwms[];

#endif // any PWMs at all
#endif // pwm_h
