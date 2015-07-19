#include <bug.h>
#include <avr/interrupt.h>

void die(const char *s __attribute__ ((progmem))) __attribute__((noreturn))
{
	cli();

	/* TODO: log the string */

	do {} while(1);
}

