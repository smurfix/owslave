#ifndef moat_h
#define moat_h

/* copied (for now) from ow_moat.h */

typedef enum {
    M_CONSOLE = 0,
    M_INPUT,
    M_OUTPUT,
    M_TEMP,
    M_HUMID,
    M_ADC,
    M_PID,
    M_PWM,
    M_MAX,
#define M_MAX M_MAX

    M_INFO = 0x70,
    M_NAME,     // port names
    M_ALERT,    // conditional search
} m_type;

#endif // moat_h
