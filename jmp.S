.global setjmp_q
setjmp_q:
         movw    r26, r24
         st      X+, r28
         st      X+, r29
         pop     r31
         pop     r30
         in      r24, 0x3d       ; 61
         st      X+, r24
         in      r24, 0x3e       ; 62
         st      X+, r24
         in      r24, 0x3f       ; 63
         st      X+, r24
         st      X+, r30
         st      X+, r31
         ijmp

.global longjmp_q
longjmp_q:
         movw    r26, r24
         ld      r28, X+
         ld      r29, X+
         ld      r30, X+
         ld      r31, X+
         ld      r0, X+
         cli
         out     0x3e, r31       ; 62
         out     0x3f, r0        ; 63
         out     0x3d, r30       ; 61
         ld      r30, X+
         ld      r31, X+
         ijmp

