#ifndef STATUS_H
#define STATUS_H

#include "dev_data.h"
#include "features.h"
#include "timer.h"

#if defined(N_STATUS)

typedef enum {
	S_boot_unknown,
	S_boot_powerup,
	S_boot_brownout,
	S_boot_watchdog,
	S_boot_external,
	S_boot_irq = 0x80,
} t_status_boot;
extern t_status_boot status_boot;

#ifdef CONDITIONAL_SEARCH
extern uint8_t init_msg;
#endif

#endif

#endif // status_h
