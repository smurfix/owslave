//owdevice - A small 1-Wire emulator for AVR Microcontroller
//
//Copyright (C) 2012  Tobias Mueller mail (at) tobynet.de
//
//This program is free software: you can redistribute it and/or modify
//it under the terms of the GNU General Public License as published by
//the Free Software Foundation, either version 3 of the License, or
// any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//
//VERSION 1.2 DS2423  for ATTINY2313 and ATTINY25

//FOR MAKE by hand
/*
avr-gcc -mmcu=[attiny25|attiny2313] -O2 -c ow_slave_DS2423.c
avr-gcc.exe -mmcu=[attiny25|attiny2313]  ow_slave_DS2423.o -o ow_slave_DS2423.elf
avr-objcopy -O ihex  ow_slave_DS2423.elf ow_slave_DS2423.hex
*/



#include <avr/io.h>
#include <avr/interrupt.h>

//does not work here because less memory by ATtiny13
#if defined(__AVR_ATtiny13A__) || defined(__AVR_ATtiny13__)
// OW_PORT Pin 6  - PB1
//Analog input PB2
#define _F_CPU 9600000

//OW Pin
#define OW_PORT PORTB //1 Wire Port
#define OW_PIN PINB //1 Wire Pin as number
#define OW_PORTN (1<<PINB1)  //Pin as bit in registers
#define OW_PINN (1<<PINB1)
#define OW_DDR DDRB  //pin direction register
#define SET_LOW() do {OW_DDR|=OW_PINN;OW_PORT&=~OW_PORTN;} while(0)  //set 1-Wire line to low
#define RESET_LOW() do {OW_DDR&=~OW_PINN;} while(0) //set 1-Wire pin as input
//Pin interrupt	
#define EN_OWINT() do {GIMSK|=(1<<INT0);GIFR|=(1<<INTF0);}while(0)  //enable interrupt 
#define DIS_OWINT() do {GIMSK&=~(1<<INT0);} while(0)  //disable interrupt
#define SET_RISING() do {MCUCR=(1<<ISC01)|(1<<ISC00);} while(0)  //set interrupt at rising edge
#define SET_FALLING() do {MCUCR=(1<<ISC01);} while(0) //set interrupt at falling edge
#define CHK_INT_EN (GIMSK&(1<<INT0))==(1<<INT0) //test if interrupt enabled
#define PIN_INT ISR(INT0_vect)  // the interrupt service routine
//Timer Interrupt
#define EN_TIMER() do {TIMSK0 |= (1<<TOIE0); TIFR0|=(1<<TOV0);} while(0) //enable timer interrupt
#define DIS_TIMER() do {TIMSK0 &= ~(1<<TOIE0);} while(0) // disable timer interrupt
#define TCNT_REG TCNT0  //register of timer-counter
#define TIMER_INT ISR(TIM0_OVF_vect) //the timer interrupt service routine

//Initializations of AVR
#define INIT_AVR() do { CLKPR=(1<<CLKPCE);\
				   CLKPR=0;/*9.6Mhz*/\
				   TIMSK0=0;\
				   GIMSK=(1<<INT0);/*set direct GIMSK register*/\
				   TCCR0B=(1<<CS00)|(1<<CS01); /*9.6mhz /64 causes 8 bit Timer countdown every 6,666us*/\
				} while(0)
#endif

#ifdef __AVR_ATtiny25__ 
// OW_PORT Pin 7  - PB2

#define _F_CPU 8000000

//OW Pin
#define OW_PORT PORTB //1 Wire Port
#define OW_PIN PINB //1 Wire Pin as number
#define OW_PORTN (1<<PINB2)  //Pin as bit in registers
#define OW_PINN (1<<PINB2)
#define OW_DDR DDRB  //pin direction register
#define SET_LOW() do { OW_DDR|=OW_PINN;OW_PORT&=~OW_PORTN;} while(0)  //set 1-Wire line to low
#define RESET_LOW() do {OW_DDR&=~OW_PINN;} while(0) //set 1-Wire pin as input
//Pin interrupt	
#define EN_OWINT() do {GIMSK|=(1<<INT0);GIFR|=(1<<INTF0);}while(0)  //enable interrupt 
#define DIS_OWINT() do {GIMSK&=~(1<<INT0);} while(0)  //disable interrupt
#define SET_RISING() do {MCUCR=(1<<ISC01)|(1<<ISC00);} while(0)  //set interrupt at rising edge
#define SET_FALLING() do {MCUCR=(1<<ISC01);} while(0) //set interrupt at falling edge
#define CHK_INT_EN (GIMSK&(1<<INT0))==(1<<INT0) //test if interrupt enabled
#define PIN_INT ISR(INT0_vect)  // the interrupt service routine
//Timer Interrupt
#define EN_TIMER() do {TIMSK |= (1<<TOIE0); TIFR|=(1<<TOV0);} while(0) //enable timer interrupt
#define DIS_TIMER() do {TIMSK  &= ~(1<<TOIE0);} while(0) // disable timer interrupt
#define TCNT_REG TCNT0  //register of timer-counter
#define TIMER_INT ISR(TIM0_OVF_vect) //the timer interrupt service routine

//Initializations of AVR
#define INIT_AVR() do { \
				   CLKPR=(1<<CLKPCE); \
				   CLKPR=0; /*8Mhz*/  \
				   TIMSK=0; \
				   GIMSK=(1<<INT0);  /*set direct GIMSK register*/ \
				   TCCR0B=(1<<CS00)|(1<<CS01); /*8mhz /64 couse 8 bit Timer interrupt every 8us*/ \
				  } while(0)
				   
#define PC_INT_VECT PCINT0_vect

#define INIT_COUNTER_PINS() do { /* Counter Interrupt */\
						GIMSK|=(1<<PCIE);\
						PCMSK=(1<<PCINT3)|(1<<PCINT4);	\
						DDRB &=~((1<<PINB3)|(1<<PINB4)); \
						istat=PINB;\
					} while(0)


#endif // __AVR_ATtiny25__ 

#if defined(__AVR_ATtiny2313A__) || defined(__AVR_ATtiny2313__)
// OW_PORT Pin 6  - PD2

#define _F_CPU 8000000

//OW Pin
#define OW_PORT PORTD //1 Wire Port
#define OW_PIN PIND //1 Wire Pin as number
#define OW_PORTN (1<<PIND2)  //Pin as bit in registers
#define OW_PINN (1<<PIND2)
#define OW_DDR DDRD  //pin direction register
#define SET_LOW() do { OW_DDR|=OW_PINN;OW_PORT&=~OW_PORTN;}while(0)  //set 1-Wire line to low
#define RESET_LOW do {OW_DDR&=~OW_PINN;} while(0) //set 1-Wire pin as input
//Pin interrupt	
#define EN_OWINT() do {GIMSK|=(1<<INT0);EIFR|=(1<<INTF0);}while(0)  //enable interrupt 
#define DIS_OWINT() do {GIMSK&=~(1<<INT0);} while(0)  //disable interrupt
#define SET_RISING() do {MCUCR|=(1<<ISC01)|(1<<ISC00);}while(0)  //set interrupt at rising edge
#define SET_FALLING() do {MCUCR|=(1<<ISC01);MCUCR&=~(1<<ISC00);} while(0) //set interrupt at falling edge
#define CHK_INT_EN (GIMSK&(1<<INT0))==(1<<INT0) //test if interrupt enabled
#define PIN_INT ISR(INT0_vect)  // the interrupt service routine
//Timer Interrupt
#define EN_TIMER() do {TIMSK |= (1<<TOIE0); TIFR|=(1<<TOV0);}while(0) //enable timer interrupt
#define DIS_TIMER() do {TIMSK &= ~(1<<TOIE0);} while(0) // disable timer interrupt
#define TCNT_REG TCNT0  //register of timer-counter
#define TIMER_INT ISR(TIMER0_OVF_vect) //the timer interrupt service routine


//Initializations of AVR
#define INIT_AVR() do { \
				   CLKPR=(1<<CLKPCE); \
				   CLKPR=0; /*8Mhz*/  \
				   TIMSK=0; \
				   GIMSK=(1<<INT0);  /*set direct GIMSK register*/ \
				   TCCR0B=(1<<CS00)|(1<<CS01); /*8mhz /64 couse 8 bit Timer interrupt every 8us*/ \
				} while(0)
				   
				   

#define PC_INT_VECT PCINT_vect

#define INIT_COUNTER_PINS() do { /* Counter Interrupt */\
						GIMSK|=(1<<PCIE);\
						PCMSK=(1<<PCINT3)|(1<<PCINT4);	\
						DDRB &=~((1<<PINB3)|(1<<PINB4)); \
						istat=PINB;\
					} while(0)


#endif // __AVR_ATtiny2313__ 

// _F_CPU is the cpufreq without external crystal
// but you can declare that your is faster (external crystal?)
#ifndef F_CPU
#define F_CPU _F_CPU
#endif
#define T_(c) ((F_CPU/64)/(1000000/c))

#define OWT_MIN_RESET T_(410)
#define OWT_RESET_PRESENCE (T_(40)-1)
#define OWT_PRESENCE T_(160)
#define OWT_READLINE (T_(35)-1)
#define OWT_LOWTIME (T_(40)-2)

//#define _ONE_DEVICE_CMDS_  //Commands for only one device on bus (Not tested)



typedef union {
	volatile uint8_t bytes[13];//={1,1,2,0,0,0,0,0,0,0,0,5,5};
	struct {
		uint16_t addr;
		uint8_t read;
		uint32_t counter;
		uint32_t zero;
		uint16_t crc;
	};
} counterpack_t;
counterpack_t counterpack;
volatile uint16_t scrc; //CRC calculation

volatile uint32_t Counter0;
volatile uint32_t Counter1;
volatile uint8_t istat;


volatile uint8_t cbuf; //Input buffer for a command
const uint8_t owid[8]={0x1D, 0xA2, 0xD9, 0x84, 0x00, 0x00, 0x02, 0x37};    
//set your own ID http://www.tm3d.de/index.php/tools/14-crc8-berechnung
volatile uint8_t bitp;  //pointer to current Byte
volatile uint8_t bytep; //pointer to current Bit

volatile uint8_t mode; //state
volatile uint8_t wmode; //if 0 next bit that send the device is  0
volatile uint8_t actbit; //current
volatile uint8_t srcount; //counter for search rom

//States / Modes
#define OWM_SLEEP 0  //Waiting for next reset pulse
#define OWM_RESET 1  //Reset pulse received 
#define OWM_PRESENCE 2  //sending presence pulse
#define OWM_READ_COMMAND 3 //read 8 bit of command
#define OWM_SEARCH_ROM 4  //SEARCH_ROM algorithms
#define OWM_MATCH_ROM 5  //test number
#define OWM_CHK_RESET 8  //waiting of rising edge from reset pulse
#define OWM_GET_ADRESS 6
#define OWM_READ_MEMORY_COUNTER 7
#define OWM_WRITE_SCRATCHPAD 9
#define OWM_READ_SCRATCHPAD 10

#ifdef _ONE_DEVICE_CMDS_
#define OWM_READ_ROM 50
#endif

//Write a bit after next falling edge from master
//its for sending a zero as soon as possible 
#define OWW_NO_WRITE 2
#define OWW_WRITE_1 1
#define OWW_WRITE_0 0



PIN_INT {
	uint8_t lwmode=wmode;  //let this variables in registers
	uint8_t lmode=mode;
	if ((lwmode==OWW_WRITE_0)) {SET_LOW(); lwmode=OWW_NO_WRITE;}    //if necessary set 0-Bit 
	DIS_OWINT(); //disable interrupt, only in OWM_SLEEP mode it is active
	switch (lmode) {
		case OWM_SLEEP:  
			TCNT_REG=~(OWT_MIN_RESET);
			EN_OWINT(); //other edges ?
			break;
		//start of reading with falling edge from master, reading closed in timer isr
		case OWM_MATCH_ROM:  //falling edge wait for receive 
		case OWM_GET_ADRESS:
		case OWM_READ_COMMAND:
			TCNT_REG=~(OWT_READLINE); //wait a time for reading
			break;
		case OWM_SEARCH_ROM:   //Search algorithm waiting for receive or send
			if (srcount<2) { //this means bit or complement is writing, 
				TCNT_REG=~(OWT_LOWTIME);
			} else 
				TCNT_REG=~(OWT_READLINE);  //init for read answer of master 
			break;
#ifdef _ONE_DEVICE_CMDS_
		case OWM_READ_ROM:
#endif		
		case OWM_READ_MEMORY_COUNTER: //a bit is sending 
			TCNT_REG=~(OWT_LOWTIME);
			break;
		case OWM_CHK_RESET:  //rising edge of reset pulse
			SET_FALLING(); 
			TCNT_REG=~(OWT_RESET_PRESENCE);  //waiting for sending presence pulse
			lmode=OWM_RESET;
			break;
	}
	EN_TIMER();
	mode=lmode;
	wmode=lwmode;
	
}			

	

TIMER_INT {
	uint8_t lwmode=wmode; //let this variables in registers
	uint8_t lmode=mode;
	uint8_t lbytep=bytep;
	uint8_t lbitp=bitp;
	uint8_t lsrcount=srcount;
	uint8_t lactbit=actbit;
	uint16_t lscrc=scrc;
	//Ask input line sate 
	uint8_t p=((OW_PIN&OW_PINN)==OW_PINN);  
	//Interrupt still active ?
	if (CHK_INT_EN) {
		//maybe reset pulse
		if (p==0) { 
			lmode=OWM_CHK_RESET;  //wait for rising edge
			SET_RISING(); 
		}
		DIS_TIMER();
	} else
	switch (lmode) {
		case OWM_RESET:  //Reset pulse and time after is finished, now go in presence state
			lmode=OWM_PRESENCE;
			SET_LOW();
			TCNT_REG=~(OWT_PRESENCE);
			DIS_OWINT();  //No Pin interrupt necessary only wait for presence is done
			break;
		case OWM_PRESENCE:
			RESET_LOW();  //Presence is done now wait for a command
			lmode=OWM_READ_COMMAND;
			cbuf=0;lbitp=1;  //Command buffer have to set zero, only set bits will write in
			break;
		case OWM_READ_COMMAND:
			if (p) {  //Set bit if line high 
				cbuf|=lbitp;
			} 
			lbitp=(lbitp<<1);
			if (!lbitp) { //8-Bits read
				lbitp=1;
				switch (cbuf) {
					case 0x55:lbytep=0;lmode=OWM_MATCH_ROM;break;
					case 0xF0:  //initialize search rom
						lmode=OWM_SEARCH_ROM;
						lsrcount=0;
						lbytep=0;
						lactbit=(owid[lbytep]&lbitp)==lbitp; //set actual bit
						lwmode=lactbit;  //prepare for writing when next falling edge
						break;
					case 0xA5:
						lmode=OWM_GET_ADRESS; //first the master send an address 
						lbytep=0;lscrc=0x7bc0; //CRC16 of 0xA5
						counterpack.bytes[0]=0;
						break;
#ifdef _ONE_DEVICE_CMDS_
					case 0xCC:
						lbytep=0;cbuf=0;lmode=OWM_READ_COMMAND;break;
					case 0x33:
						lmode=OWM_READ_ROM;
						lbytep=0;	
						break;
#endif											
					default: lmode=OWM_SLEEP;  //all other commands do nothing
				}		
			}			
			break;
		case OWM_SEARCH_ROM:
			RESET_LOW();  //Set low also if nothing send (branch takes time and memory)
			lsrcount++;  //next search rom mode
			switch (lsrcount) {
				case 1:lwmode=!lactbit;  //preparation sending complement
					break;
				case 3:
					if (p!=(lactbit==1)) {  //check master bit
						lmode=OWM_SLEEP;  //not the same go sleep
					} else {
						lbitp=(lbitp<<1);  //prepare next bit
						if (lbitp==0) {
							lbitp=1;
							lbytep++;
							if (lbytep>=8) {
								lmode=OWM_SLEEP;  //all bits processed 
								break;
							}
						}				
						lsrcount=0;
						lactbit=(owid[lbytep]&lbitp)==lbitp;
						lwmode=lactbit;
					}		
					break;			
			}
			break;
		case OWM_MATCH_ROM:
			if (p==((owid[lbytep]&lbitp)==lbitp)) {  //Compare with ID Buffer
				lbitp=(lbitp<<1);
				if (!lbitp) {
					lbytep++;
					lbitp=1;
					if (lbytep>=8) {
						lmode=OWM_READ_COMMAND;  //same? get next command
						
						cbuf=0;
						break;			
					}
				} 
			} else {
				lmode=OWM_SLEEP;
			}
			break;
		case OWM_GET_ADRESS:  
			if (p) { //Get the Address for reading
				counterpack.bytes[lbytep]|=lbitp;
			}  
			//address is part of crc
			if ((lscrc&1)!=p) lscrc=(lscrc>>1)^0xA001; else lscrc >>=1;
			lbitp=(lbitp<<1);
			if (!lbitp) {	
				lbytep++;
				lbitp=1;
				if (lbytep==2) {
					lmode=OWM_READ_MEMORY_COUNTER;
					lactbit=(lbitp&counterpack.bytes[lbytep])==lbitp;
					lwmode=lactbit;
					lsrcount=(counterpack.addr&0xfe0)+0x20-counterpack.addr; 
					//bytes between start and Counter Values, Iam never understanding why so much???
					break;
				} else counterpack.bytes[lbytep]=0;
			}			
			break;	
		case OWM_READ_MEMORY_COUNTER:
			RESET_LOW();
			//CRC16 Calculation
			if ((lscrc&1)!=lactbit) lscrc=(lscrc>>1)^0xA001; else lscrc >>=1;
			p=lactbit;
			lbitp=(lbitp<<1);
			if (!lbitp) {		
				lbytep++;
				lbitp=1;
				if (lbytep==3) {
					lsrcount--;
					if (lsrcount) lbytep--;
					else  {//now copy counter in send buffer
						switch (counterpack.addr&0xFe0) {
						case 0x1E0:
							counterpack.counter=Counter0;
							break;
						case 0x1C0:
							counterpack.counter=Counter1;
							break;
						default: counterpack.counter=0;
						}
					}
				}
				if (lbytep>=13) { //done sending
					lmode=OWM_SLEEP;
					break;			
				}  		 
				if ((lbytep==11)&&(lbitp==1)) { //Send CRC
					counterpack.crc=~lscrc; 
				}			
					 
			}					
			lactbit=(lbitp&counterpack.bytes[lbytep])==lbitp;
			lwmode=lactbit;
			
			break;
#ifdef _ONE_DEVICE_CMDS_	
		case OWM_READ_ROM:
			RESET_LOW();
			lbitp=(lbitp<<1);
			if (!lbitp) {		
				lbytep++;
				lbitp=1;
				if (lbytep>=8) {
					lmode=OWM_SLEEP;
					break;			
				} 
			}					
			lactbit=(lbitp&owid[lbytep])==lbitp;
			lwmode=lactbit;
			break;
#endif		
		}
		if (lmode==OWM_SLEEP)
			DIS_TIMER();
		if (lmode!=OWM_PRESENCE)  { 
			TCNT_REG=~(OWT_MIN_RESET-OWT_READLINE);  //OWT_READLINE around OWT_LOWTIME
			EN_OWINT();
		}
		mode=lmode;
		wmode=lwmode;
		bytep=lbytep;
		bitp=lbitp;
		srcount=lsrcount;
		actbit=lactbit;
		scrc=lscrc;
}



ISR(PC_INT_VECT) {
	if (((PINB&(1<<PINB3))==0)&&((istat&(1<<PINB3))==(1<<PINB3))) {	Counter0++;	}
	if (((PINB&(1<<PINB4))==0)&&((istat&(1<<PINB4))==(1<<PINB4))) {	Counter1++;	}
	istat=PINB;
}



int main(void) {
	mode=OWM_SLEEP;
	wmode=OWW_NO_WRITE;
	OW_DDR&=~OW_PINN;
	
	uint8_t i;
	for(i=0;i<sizeof(counterpack);i++) counterpack.bytes[i]=0;
	Counter0=0;
	Counter1=0;

	
	SET_FALLING();
	
	INIT_AVR();
	DIS_TIMER();
	
	INIT_COUNTER_PINS();

	sei();
	
	while(1){
		
	}
}	
