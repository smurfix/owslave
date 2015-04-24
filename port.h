#ifndef PORT_H
#define PORT_H

#include "dev_data.h"
#include "features.h"

#if defined(N_PORT)

typedef struct {
	uint8_t adr;
	uint8_t flags;
} t_port;
extern t_port ports[];

#define _P_VARS(_port) \
	uint8_t flg __attribute__((unused)) = _port->flags; \
	uint8_t adr = _port->adr; \
	uint8_t _i=3*(adr>>3); \
	uint8_t *pin __attribute__((unused)) = (uint8_t *)(0x20+_i); \
	uint8_t *ddr __attribute__((unused)) = (uint8_t *)(0x20+_i+1); \
	uint8_t *port __attribute__((unused)) = (uint8_t *)(0x20+_i+2); \
	adr = 1<<(adr & 0x07);

#define _P_GET(_reg) (!!(*_reg & adr))
#define _P_SET(_reg,_val) do { \
	if(_val)                   \
		*_reg |= adr;         \
	else                       \
		*_reg &=~adr;         \
	} while(0);

// the first two bits are used for t_port_out, i.e. PO_* constants. Hardcoded.
#define PFLG_ALERT   (1<<2)  // alert when port changes externally
#define PFLG_ALT     (1<<3)  // switch H/Z and L/pull-up; default: H/L and Z/pull-up
#define PFLG_ALT2    (1<<4)  // switch H/pullup and L/Z
#define PFLG_POLL    (1<<5)  // change has been reported
#define PFLG_CHANGED (1<<6)  // pin change
#define PFLG_CURRENT (1<<7)  // last-seen value. Hardcoded.

typedef enum {
	PI_OFF=0, PI_ON=1
} t_port_in;
typedef enum {
	PO_OFF=0, PO_ON=1, PO_Z=2, PO_PULLUP=3
} t_port_out;

// actual port-pin state
static inline t_port_in port_get_in(t_port *portp) {
	_P_VARS(portp)
	return _P_GET(pin);
}

// read intended port state from registers
static inline t_port_out port_get_out(t_port *portp) {
	_P_VARS(portp)
	return _P_GET(port) | (!_P_GET(ddr)<<1) ;
}

// set intended port state
static inline void port_set_out(t_port *portp, t_port_out state) {
	_P_VARS(portp)
	_P_SET(port, state&1);
	_P_SET(ddr,!(state&2));
	portp->flags = (portp->flags&~PFLG_CURRENT) | (state<<7);
}

// update flags based on current port state
void port_check(t_port *pp);

// Set port to 0/1 according to mode (PFLG_ALT*). This is harder than it seems.
void port_set(t_port *portp, char val);


static inline char port_changed(t_port *portp) {
	return portp->flags & PFLG_CHANGED;
}

/* Number of highest port that has a change +1  */
EXTERN uint8_t port_changed_cache;

/* Note whether a port has changed */
static inline char port_has_changed(t_port *portp) {
	uint8_t flg = portp->flags;
	if (flg & PFLG_CHANGED) {
		flg |= PFLG_POLL;
		flg &=~ PFLG_CHANGED;
		portp->flags = flg;
		return 1;
	}
	return 0;
}

static inline void port_pre_send (t_port *portp) {
	(void)port_has_changed(portp);
}

static inline void port_post_send (t_port *portp) {
	uint8_t flg = portp->flags;
	if(flg & PFLG_POLL) {
		if (flg & PFLG_CHANGED)
			flg &=~PFLG_CHANGED;
		else
			flg &=~PFLG_POLL;
		portp->flags = flg;
	}
}

/* Called after reporting changes, clears PFLG_POLL when PFLG_CHANGED is off */
void poll_clear(void);

void port_init(void);
void port_poll(void);

#ifdef CONDITIONAL_SEARCH
static inline char port_alert(void) {
	if (port_changed_cache)
		return 1;
	return 0;
}

#else
#define port_alert() 0
#endif

#else // no i/o

#define port_init() do {} while(0)
#define port_poll() do {} while(0)
#define port_alert() 0

#endif // any inputs or outputs at all
#endif // port_h
