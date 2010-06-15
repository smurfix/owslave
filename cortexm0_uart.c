#include "features.h"

#ifdef HAVE_UART
#include "uart.h"

/** Size of the circular transmit buffer, must be power of 2 */
#ifndef UART_TX_BUFFER_SIZE
#define UART_TX_BUFFER_SIZE 512
#endif
#ifndef POLLED_TRANSMITTER
#error "for Cortex M0 define POLLED_TRANSMITTER!"
#endif

/* some LPC111x uart bits */
#define IER_RBR		0x01
#define IER_THRE	0x02
#define IER_RLS		0x04


/* module global variables (init to 0) */
static volatile unsigned char UART_TxBuf[UART_TX_BUFFER_SIZE];
static volatile unsigned char UART_TxHead;
static volatile unsigned char UART_TxTail;

/* helper to store a byte from the transmitter ring to the UART TX register */
static inline void uart_send(void)
{
	unsigned char tmptail;

	if (UART_TxHead != UART_TxTail) {
		/* calculate and store new buffer index */
		tmptail = (UART_TxTail + 1) & UART_TX_BUFFER_MASK;
		/* get one byte from buffer and write it to UART */
		LPC_UART->THR = UART_TxBuf[tmptail];  /* start transmission */
		UART_TxTail = tmptail;
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
	volatile u_long r;

	/* disable uart interrupts, we don't use it */
	NVIC_DisableIRQ(UART_IRQn);
	LPC_UART->IER = 0;

	/* setup IO configuration, pins+function */
	LPC_IOCON->PIO1_6 &= ~0x07; LPC_IOCON->PIO1_6 |= 0x01;	// RXD
	LPC_IOCON->PIO1_7 &= ~0x07; LPC_IOCON->PIO1_7 |= 0x01;	// TXD

	/* enable UART clock */
	LPC_SYSCON->SYSAHBCLKCTRL |= (1<<12);
	LPC_SYSCON->UARTCLKDIV = 0x1;     /* divided by 1 */

	LPC_UART->LCR = 0x83;             /* 8 bits, no Parity, 1 Stop bit */
	r = LPC_SYSCON->UARTCLKDIV;

	/* divider value */
	div = (((SystemCoreClock * LPC_SYSCON->SYSAHBCLKDIV) / r) / 16) / baudrate ;

	LPC_UART->DLM = div / 256;
	LPC_UART->DLL = div % 256;
	LPC_UART->LCR = 0x03;				/* DLAB = 0 */
	LPC_UART->FCR = 0x07;				/* enable and reset TX and RX FIFO. */

#if 0
	/* read to clear the line status. */
	r = LPC_UART->LSR;

	/* ensure a clean start, no data in either TX or RX FIFO. */
	while((LPC_UART->LSR & (LSR_THRE | LSR_TEMT)) != (LSR_THRE|LSR_TEMT)) ;
	while(LPC_UART->LSR & LSR_RDR)
	  r = LPC_UART->RBR;
#endif
}

void uart_putc(unsigned char data)
{
    unsigned char tmphead;

	if(data == '\n')
		uart_putc('\r');

    /* if full drop character, otherwise put to tail */
    tmphead  = (UART_TxHead + 1) & UART_TX_BUFFER_MASK;
    if (tmphead != UART_TxTail) {
		UART_TxBuf[tmphead] = data;
		UART_TxHead = tmphead;
    }
}

void uart_puts(const char *s )
{
    while (*s)
      uart_putc(*s++);

}

void uart_puthex_nibble(const unsigned char b)
{
    unsigned char  c = b & 0x0f;
    if (c>9) c += 'A'-10;
    else c += '0';
    uart_putc(c);
}

void uart_puthex_byte_(const unsigned char b)
{
    uart_puthex_nibble(b>>4);
    uart_puthex_nibble(b);
}

void uart_puthex_byte(const unsigned char  b)
{
    if(b & 0xF0)
        uart_puthex_nibble(b>>4);
    uart_puthex_nibble(b);
}
void uart_puthex_word(const unsigned short b)
{
    if (b&0xFF00) {
        uart_puthex_byte(b>>8);
        uart_puthex_byte_(b);
    } else {
        uart_puthex_byte(b);
    }
}
#else
void uart_try_send(void) {}
#endif // HAVE_UART

