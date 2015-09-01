/* stupid workaround */
#ifndef PGM_H
#define PGM_H
#include <avr/pgmspace.h>
#ifndef pgm_read_ptr
#define pgm_read_ptr(address_short)     pgm_read_ptr_near(address_short)
#endif
#endif

