/*CRC */
volatile uint8_t vbus_crc;

/* Anzahl Data Frames */
volatile uint8_t vbus_framecount;
volatile uint8_t vbus_framepointer;


/* Data buffers */
#define VBUS_OUTBUFFERSIZE 32
volatile uint8_t vbus_out_buffer[VBUS_OUTBUFFERSIZE];
volatile uint8_t vbus_in_buffer[4];
volatile uint8_t vbus_out_buffer_ptr;

/* VBUS Codes */

#define VBUS_SYNC_CHAR 0xAA

/* Vbus State Machine */
volatile uint8_t vbus_state;

/* State Machine states */
#define VBUS_IDLE 0x00
#define VBUS_SYNC 0x10
#define VBUS_DATA 0x20
