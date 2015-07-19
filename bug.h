#ifndef BUG_H
#define BUG_H

#define __linktime_error(message) __attribute__((__error__(message)))

#ifndef __OPTIMIZE__
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))
#else

#ifndef P
#define P(s) ({static const char c[] __attribute__ ((progmem)) = s;c;})
#endif

extern void __build_bug_failed(void);
#define BUILD_BUG_ON(condition)                                 \
        do {                                                    \
                ((void)sizeof(char[1 - 2*!!(condition)]));      \
                if (condition) __build_bug_failed();       \
        } while(0)
#endif

/**
 * BUILD_BUG - break compile if used.
 *
 * If you have some code that you expect the compiler to eliminate at
 * build time, you should use BUILD_BUG to detect if it is
 * unexpectedly used.
 */
#define BUILD_BUG()                                             \
        do {                                                    \
                __build_bug_failed();                      \
        } while (0)
	
#ifdef DEBUG
// #include <debug.h>
extern void die(const char *s __attribute__((progmem)));
#define BUG(s) die(P(s))
#else
#define BUG(s) do {} while(1) // depends on watchdog
#endif // !DEBUG

#endif // LIB_BUG
