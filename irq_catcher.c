#include "onewire.h"
#include "features.h"

#ifdef HAVE_IRQ_CATCHER

void pingi(uint8_t c) {
    uint32_t j;
    DBG_IN();DBG_OFF();
    for(j=0;j<20;j++) ;
    DBG_T(c);
    for(j=0;j<1000000;j++) ;

    extern void __ctors_end(void); __ctors_end();
}

void __vector_1(void) __attribute__((signal,weak));
void __vector_2(void) __attribute__((signal));
void __vector_3(void) __attribute__((signal));
void __vector_4(void) __attribute__((signal,weak));
void __vector_5(void) __attribute__((signal));
void __vector_6(void) __attribute__((signal));
void __vector_7(void) __attribute__((signal));
void __vector_8(void) __attribute__((signal));
void __vector_9(void) __attribute__((signal));
void __vector_10(void) __attribute__((signal));
void __vector_11(void) __attribute__((signal));
void __vector_12(void) __attribute__((signal));
void __vector_13(void) __attribute__((signal));
void __vector_14(void) __attribute__((signal));
void __vector_15(void) __attribute__((signal));
void __vector_16(void) __attribute__((signal,weak));
void __vector_17(void) __attribute__((signal));
void __vector_18(void) __attribute__((signal,weak));
void __vector_19(void) __attribute__((signal,weak));
void __vector_20(void) __attribute__((signal));
void __vector_21(void) __attribute__((signal));
void __vector_22(void) __attribute__((signal));
void __vector_23(void) __attribute__((signal));
void __vector_24(void) __attribute__((signal));
void __vector_25(void) __attribute__((signal));
void __vector_26(void) __attribute__((signal));
void __vector_27(void) __attribute__((signal));
void __vector_28(void) __attribute__((signal));
void __vector_29(void) __attribute__((signal));
void __vector_30(void) __attribute__((signal));
void __vector_31(void) __attribute__((signal));

void __vector_1(void) { pingi(1); }
void __vector_2(void) { pingi(2); }
void __vector_3(void) { pingi(3); }
void __vector_4(void) { pingi(4); }
void __vector_5(void) { pingi(5); }
void __vector_6(void) { pingi(6); }
void __vector_7(void) { pingi(7); }
void __vector_8(void) { pingi(8); }
void __vector_9(void) { pingi(9); }
void __vector_10(void) { pingi(10); }
void __vector_11(void) { pingi(11); }
void __vector_12(void) { pingi(12); }
void __vector_13(void) { pingi(13); }
void __vector_14(void) { pingi(14); }
void __vector_15(void) { pingi(15); }
void __vector_16(void) { pingi(16); }
void __vector_17(void) { pingi(17); }
void __vector_18(void) { pingi(18); }
void __vector_19(void) { pingi(19); }
void __vector_20(void) { pingi(20); }
void __vector_21(void) { pingi(21); }
void __vector_22(void) { pingi(22); }
void __vector_23(void) { pingi(23); }
void __vector_24(void) { pingi(24); }
void __vector_25(void) { pingi(25); }
void __vector_26(void) { pingi(26); }
void __vector_27(void) { pingi(27); }
void __vector_28(void) { pingi(28); }
void __vector_29(void) { pingi(29); }
void __vector_30(void) { pingi(30); }
void __vector_31(void) { pingi(31); }

#endif

