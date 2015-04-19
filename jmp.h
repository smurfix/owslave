#ifndef JMP_H
#define JMP_H

/*
   q_jmp_buf:
	offset	size	description
	0	 2	frame pointer (r29:r28)
	2	 2	stack pointer (SPH:SPL)
	4	 1	status register (SREG)
	5	 2/3	return address (PC) (2 bytes used for <=128Kw flash)
	7        = total size (AVR Tiny10 family always has 2 bytes PC)
 */

#if	defined(__AVR_3_BYTE_PC__) && __AVR_3_BYTE_PC__
# define _QJB_LEN  8
#else
# define _QJB_LEN  7
#endif
typedef struct _q_jmp_buf { unsigned char _buf[_QJB_LEN]; } q_jmp_buf[1];

/* Note that this version of setjmp/longjmp does not bother with
 * passing a value. This is intentional.
 */
extern void setjmp_q(q_jmp_buf _buf);
extern void longjmp_q(q_jmp_buf _buf) __attribute__((__noreturn__));

#endif  /* JMP_H */
