/*
 * very basic CortexM0 uart implementation, no interrupt support
 * no receiver support, just enough for debug messages.
 */

#include "features.h"

#ifdef HAVE_UART
#include "uart.h"

/** Size of the circular transmit buffer, must be power of 2 */
#ifndef UART_TX_BUFFER_SIZE
#define UART_TX_BUFFER_SIZE 512
#endif

#define UART_TX_BUFFER_MASK ( UART_TX_BUFFER_SIZE - 1)

#if ( UART_TX_BUFFER_SIZE & UART_TX_BUFFER_MASK )
 #error "TX buffer size is not a power of 2"
#endif


#ifndef POLLED_TRANSMITTER
#error "for Cortex M0 define POLLED_TRANSMITTER!"
#endif

/* some LPC111x uart bits */
#define IER_RBR		0x01
#define IER_THRE	0x02
#define IER_RLS		0x04
#define LSR_THRE	0x20

/* module global variables (init to 0) */
static unsigned char txbuf[UART_TX_BUFFER_SIZE];
static u_long head;
static u_long tail;

/* helper to store a byte from the transmitter ring to the UART TX register */
static inline void uart_send(void)
{
	u_long tmptail;

	if (head != tail) {
		/* calculate and store new buffer index */
		tmptail = (tail + 1) & UART_TX_BUFFER_MASK;
		/* get one byte from buffer and write it to UART */
		LPC_UART->THR = txbuf[tmptail];
		tail = tmptail;
	}
}

/* try to empty the transmit buffer, checks if transmitter done first */
void uart_try_send(void)
{
	if(LPC_UART->LSR & LSR_THRE)
		uart_send();
}

/* baudrate is 'int' for portability reasons */
void uart_init(unsigned int baudrate)
{
	u_long div;
//	volatile u_long r;

	/* disable uart interrupt, we don't use it */
	NVIC_DisableIRQ(UART_IRQn);
	LPC_UART->IER = 0;

	/* setup IO configuration, pins+function */
	LPC_IOCON->PIO1_6 &= ~0x07; LPC_IOCON->PIO1_6 |= 0x01;	// RXD
	LPC_IOCON->PIO1_7 &= ~0x07; LPC_IOCON->PIO1_7 |= 0x01;	// TXD

	/* enable UART clock */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1<<12);
	LPC_SYSCON->UARTCLKDIV = 0x1;     /* divided by 1 */

	LPC_UART->LCR = 0x83;             /* 8 bits, no Parity, 1 Stop bit */
//	r = LPC_SYSCON->UARTCLKDIV;

	/* divider value */
	div = (((SystemCoreClock * LPC_SYSCON->SYSAHBCLKDIV)
					/ LPC_SYSCON->UARTCLKDIV) / 16) / baudrate ;

	LPC_UART->DLM = div / 256;
	LPC_UART->DLL = div % 256;
	LPC_UART->LCR = 0x03;				/* DLAB = 0 */
	LPC_UART->FCR = 0x07;				/* enable and reset TX and RX FIFO. */
}

void uart_putc(u_char b)
{
    u_long tmphead;

	if(b == '\n')
		uart_putc('\r');

    /* if full drop character, otherwise put to tail */
    tmphead = (head + 1) & UART_TX_BUFFER_MASK;
    if (tmphead != tail) {
		txbuf[tmphead] = b;
		head = tmphead;
    }
}

void uart_puts(const char *s )
{
    while (*s)
      uart_putc(*s++);
}

void uart_puthex_nibble(u_char b)
{
    b &= 0x0f;
    b += b > 9 ? 'A'-10 : '0';
    uart_putc(b);
}

void uart_puthex_byte_(u_char b)
{
    uart_puthex_nibble(b >> 4);
    uart_puthex_nibble(b);
}

void uart_puthex_byte(u_char b)
{
    if(b & 0xF0)
        uart_puthex_nibble(b >> 4);
    uart_puthex_nibble(b);
}

void uart_puthex_word(u_short b)
{
    if (b & 0xFF00) {
        uart_puthex_byte(b >> 8);
        uart_puthex_byte_(b);
    } else
        uart_puthex_byte(b);
}

void uart_puthex_long(u_long b)
{
    if (b & 0xFFFF0000) {
        uart_puthex_word(b >> 8);
        uart_puthex_word(b);
    } else
        uart_puthex_word(b);
}
#else
void uart_try_send(void) {}
#endif // HAVE_UART

