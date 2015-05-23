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
} t_status_boot;
extern t_status_boot status_boot;

typedef enum {
	S_reboot = 1,
	S_max
#define STATUS_MAX S_max
} t_status_nr;

extern t_status_boot status_boot;

#ifdef CONDITIONAL_SEARCH
extern uint8_t init_msg;
#endif

#endif

#endif // status_h
