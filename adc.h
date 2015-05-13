#ifndef ADC_H
#define ADC_H

#include "dev_data.h"
#include "features.h"

#if defined(N_ADC)

typedef struct {
	uint8_t flags;
	uint16_t value;
	uint16_t lower;
	uint16_t upper;
} adc_t;
extern adc_t adcs[];

// the first two bits are used for adc_out_t, i.e. PO_* constants. Hardcoded.
#define ADC_MASK     ((1<<3)-1)
#define ADC_VBG      0x01
#define ADC_VGND     0x02
#define ADC_VTEMP    0x03
#define ADC_ALT      (1<<3)  // use Vbg et al, not analog inputs 0-7
#define ADC_REF      (1<<4)  // use Vbg as reference, not Vdd
#define ADC_ALERT    (1<<6)  // trigger an alarm when stepping over boundary
#define ADC_IS_ALERT (1<<7)  // alarm triggered?

void adc_init(void);
void adc_poll(void);

#ifdef CONDITIONAL_SEARCH
/* Number of highest adc that has a change +1  */
EXTERN uint8_t adc_changed_cache;

static inline char adc_alert(void) {
	if (adc_changed_cache)
		return 1;
	return 0;
}

#else
#define adc_alert() 0
#endif

#else // no i/o

#define adc_init() do {} while(0)
#define adc_poll() do {} while(0)
#define adc_alert() 0

#endif // any inputs or outputs at all
#endif // adc_h
