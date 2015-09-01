#ifndef CONSOLE_H
#define CONSOLE_H

#include "pgm.h"
#include "features.h"

#ifndef P
#define P(s) ({static const char c[] __attribute__ ((progmem)) = s;c;})
#endif

#ifdef N_CONSOLE
#if N_CONSOLE != 1
#error "I only know how to do a single console"
#endif

void console_init(void);
extern void console_putc(unsigned char data);

extern void console_puts(const char *s);
extern void console_puts_p(const char *s);
#define console_puts_P(__s) console_puts_p(P(__s))
extern void console_puti(int i);
extern void console_putl(long i);
extern void console_puthex_nibble(const unsigned char b);
extern void console_puthex_byte(const unsigned char b);
extern void console_puthex_byte_(const unsigned char b);
extern void console_puthex_word(const uint16_t b);

extern uint8_t console_buf_len(void);
extern uint8_t console_buf_read(unsigned char *addr, uint8_t len);
extern void console_buf_done(uint8_t len);

#else

#define console_init() do{}while(0)
#define console_putc(x) do{}while(0)
#define console_puts(x) do{}while(0)
#define console_puts_p(x) do{}while(0)
#define console_puts_P(x) do{}while(0)
#define console_puti(x) do{}while(0)
#define console_putl(x) do{}while(0)
#define console_puthex_nibble(x) do{}while(0)
#define console_puthex_byte(x) do{}while(0)
#define console_puthex_byte_(x) do{}while(0)
#define console_puthex_word(x) do{}while(0)
#define console_buf_len() 0
#define console_buf_read(x,y) 0
#define console_buf_done(y) do{}while(0)

#endif // !TC_CONSOLE

#define console_alert() (!!console_buf_len())

#endif // CONSOLE_H 

