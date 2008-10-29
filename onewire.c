
#include <avr/io.h>
#include <avr/interrupt.h>


#define F_CPU                 8000000

// Prototypen der Funktionen
void ioinit();

// Fuer One Wire Interface
volatile unsigned char status_global, bitcount, transbyte, bytecount ;
volatile unsigned char shift_reg, fb_bit ;
volatile unsigned char transdata[9];   // Hier werden die Daten aufbewahrt, die gesendet/empfangen werden,
					// dabei gilt die Zuordnung wie beim 18B20 Tempsensor. Auch fuer die EEPROM Bytes

// Definitionen für den Zustandsanzeiger 
#define IDLE 0x00
#define RESET 0x01
#define PRESENCEPULSE 0x02
#define WAITOPCODE 0x04
#define READMEM 0x08
#define WRITEEEPROM 0x09
#define RECEIVE_OPCODE 0x10
#define MATCHROM 0x20
#define SEARCHROM 0x40
#define WRITEMEM 0x80

// definitionen für die OneWire erkennung
#define FAMILYCODE 0x28       // Identifies as 18B20 Thermometer
#define DEVICEID  0x55        // Um Speicher zu sparen wird die deviceid 5 mal gesendet, es können also 256 IDs eingestellt werden
#define CRCVAL  0x54   // aus http://www.zorc.breitbandkatze.de/crc.html
   // Mit order = 8, Polynom 0x31, Initial 00, final 0  data: %aa%aa%aa%aa%aa%aa%10
                           // Ãœbertragen wird CRC 6mal DEVICEID FAMILYCODE
#define ADIN0 0x02
#define ADIN1 0x03

#ifdef __AVR_ATtiny13__
#define PAD_RELAIS  0        
#define ONEWIREPIN 1         // Pin, an dem der 1-Wire angeschlossen ist, MUSS INT0 sein
#define PRESENCEZEIT 15      // Timingdeclarationen für 9,6 MHz
#define PRESENCEWAITZEIT 2
#define ABTASTZEIT 3           // Abtastzeitpunkt beim empfangen 
#define SENDEZEIT 5           //  Dauer des 0-Sendeimpulses

#elif defined (__AVR_ATmega8__)
#define TIMSK0 TIMSK        // TIMSK0 istTIMSK beim Attiny13
#define TIFR0 TIFR
#define EEPE EEWE
#define EEMPE EEMWE

#define PAD_RELAIS  3           // 
#define ONEWIREPIN 2        // Pin, an dem der 1-Wire angeschlossen ist, MUSS INT0 sein
#define PRESENCEZEIT 15
#define PRESENCEWAITZEIT 2
#define ABTASTZEIT 3           // War 5 jetzt 3
#define SENDEZEIT 5           // War 15 jetzt auf 10
#define BAUDRATE 38400

#endif    // Port als Ausgang


// Initialisierung der Hardware
void ioinit()
{
unsigned char i;
#ifdef __AVR_ATtiny13__
    CLKPR = 0x80;     // Per software auf 9,6 MHz stellen Aenderung des Vorteilers freigeben
    CLKPR = 0x00;     // Vorteiler auf 1 (000) setzen
    PORTB &= ~(1 << ONEWIREPIN);    // Pin auf Null, aber Input
    PORTB |= (1 << PAD_RELAIS);    // Pullup  setzen    
    DDRB |= (1 << PAD_RELAIS);     // Ausgang enable

    TCCR0A = 0;
    TCCR0B = 0x03;    // Prescaler 1/64

    MCUCR |= (1 << ISC00);          // Interrupt auf beide Flanken

    GIMSK |= (1 << INT0) ;      // Externen Interrupt freigeben


    // Den ADC aktivieren und Teilungsfaktor auf 64 stellen
    ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1);
       //      ADMUX |= (1<<ADLAR);    // Externe Referenz, Obere 8 Bit alignen (untere zwei Bit wegschmeissen)
    // Get data from EEPROM
    while(EECR & (1<<EEPE));     // Wait for previous write to finish
    for (i=2; i<4; i++){
      /* Set up address register */
      EEARL = i;               // set EPROM Address
      /* Start eeprom read by writing EERE */
      EECR |= (1<<EERE);
      /* Return data from data register */
      transdata[i] =  EEDR;
    }


#elif defined (__AVR_ATmega8__)
/* Code fuer Mega8 und Mega32 */ 
// Clock wird ueber Fuses eingestellt:
// CKSEL = 0100;   Fuse Low Byte Bits 3:0
    PORTD &= ~(1 << ONEWIREPIN);    // Pin auf Null, aber Input
    PORTD |= (1 << PAD_RELAIS);    // Pullup  setzen    
    DDRD |= (1 << PAD_RELAIS);     // Ausgang enable

    // Initialisiert Timer1, um jede Sekunde 1000 IRQs auszulÃ¶sen
    // ATmega: Mode #4 fÃ¼r Timer1 und voller MCU-Takt (Prescale=1)
    TCCR0 = 0x03;    // Prescaler 1/64

    MCUCR |= (1 << ISC00);          // Interrupt auf beide Flanken

    GIMSK |= (1 << INT0) ;      // Externen Interrupt freigeben


    // Den ADC aktivieren und Teilungsfaktor auf 64 stellen
    ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1);
       //      ADMUX |= (1<<ADLAR);    // Externe Referenz, Obere 8 Bit alignen (untere zwei Bit wegschmeissen)
    // Get data from EEPROM
    while(EECR & (1<<EEPE));     // Wait for previous write to finish
    for (i=2; i<4; i++){
      /* Set up address register */
      EEARL = i;               // set EPROM Address
      /* Start eeprom read by writing EERE */
      EECR |= (1<<EERE);
      /* Return data from data register */
      transdata[i] =  EEDR;
    }

    uint16_t ubrr = (uint16_t) ((uint32_t) F_CPU/(16*BAUDRATE) - 1);
     
    UBRRH = (uint8_t) (ubrr>>8);
    UBRRL = (uint8_t) (ubrr);
   
    // UART Receiver und Transmitter anschalten
    // Data mode 8N1, asynchron
    UCSRB = (1 << RXEN) | (1 << TXEN);
    UCSRC = (1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0);

    // Flush Receive-Buffer
    do
    {
        uint8_t dummy;
        (void) (dummy = UDR);
    }
    while (UCSRA & (1 << RXC));
#else
#error Das ist noch nicht implementiert fÃ¼r diesen Controller!
#endif    // Port als Ausgang

}

#if defined (__AVR_ATmega8__)

unsigned char uart_putc (unsigned char c)
{
    // Warten, bis UDR bereit ist fÃ¼r einen neuen Wert
    while (!(UCSRA & (1 << UDRE)))
        ;

    // UDR schreiben startet die Ãœbertragung      
    UDR = c;

    return 1;
}

unsigned char uart_getc_wait()
{
    // Warten, bis etwas empfangen wird
    while (!(UCSRA & (1 << RXC)))
        ;

    // Das empfangene Zeichen zurÃ¼ckliefern   
    return UDR;
}

unsigned char uart_getc_nowait()
{
    // Liefer das empfangene Zeichen, falls etwas empfangen wurde; -1 sonst
    return (UCSRA & (1 << RXC)) ? (int) UDR : -1;
}
#endif    



// Das Hauptprogramm (Einsprungpunkt)
int main()
{
    unsigned char i, dummy;
    unsigned char hyst;
    // Peripherie initialisieren
    ioinit();
    status_global = IDLE;
    unsigned char blind;
    // Interrupts aktivieren

    sei();
//                PORTD |= (1 << PAD_RELAIS);    // Debug Relais  an 

    // Eine Endlosschleife.
    while (1)
    {
#if defined (__AVR_ATmega8__)
    switch (status_global){                            // Nur zum Debuggen
	case RESET: uart_putc('R'); 
	case PRESENCEPULSE: uart_putc('P'); 
	case WAITOPCODE: uart_putc('W'); 
	case READMEM: uart_putc('M'); 
	case WRITEEEPROM: uart_putc('W'); 
	case RECEIVE_OPCODE: uart_putc('O'); 
	case MATCHROM: uart_putc('M'); 
//	case SEARCHROM: uart_putc('S'); 
	case WRITEMEM: uart_putc('I'); 
	case IDLE: ; 
//	default: {
//		uart_putc(status_global);
 //               status_global = IDLE;
  //            }
}
#endif    

             // Kanal des Multiplexers waehlen
             ADMUX = ADIN0;
     //        ADMUX |= (1<<ADLAR);    // Externe Referenz, Obere 8 Bit alignen (untere zwei Bit wegschmeissen)

             // Den ADC initialisieren und einen sog. Dummyreadout machen
             ADCSRA |= (1<<ADSC);
             while(ADCSRA & (1<<ADSC));

             //  Wandlung starten
             ADCSRA |= (1<<ADSC);
                   // Auf Ergebnis warten...
             while(ADCSRA & (1<<ADSC));

             transdata[0] = ADCL;    // Oberes Byte auslesen
             dummy = ADCH;    // Oberes Byte auslesen
             // Kanal des Multiplexers waehlen
             ADMUX = ADIN1;

             // Den ADC initialisieren und einen sog. Dummyreadout machen
             ADCSRA |= (1<<ADSC);
             while(ADCSRA & (1<<ADSC));

             //  Wandlung starten
             ADCSRA |= (1<<ADSC);
                   // Auf Ergebnis warten...
             while(ADCSRA & (1<<ADSC));

             transdata[1] = ADCL;    // Oberes Byte auslesen
             dummy = ADCH;    // Oberes Byte auslesen

             if (transdata[0] > transdata[1] + transdata[2] - hyst){
      //          PORTD |= (1 << PAD_RELAIS);    // Relais  an 
		hyst = transdata[3];
	     }
	     else {   
        //       PORTD &= ~(1 << PAD_RELAIS);    // Relais aus
	       hyst = 0;
	     }
             transdata[4] = hyst;	
     }
}


// Timer interrupt routine
#ifdef __AVR_ATtiny13__
ISR (TIM0_OVF_vect)
#elif defined (__AVR_ATmega8__)
ISR (TIMER0_OVF_vect)
#endif    // Port als Ausgang
{
    unsigned char tim0_i, status;
    TIMSK0 &= ~(1 << TOIE0);       // Timer Interrupt aus
    status = status_global;
    if (status == RESET){
#ifdef __AVR_ATtiny13__
          DDRB |= (1 << ONEWIREPIN);    // Pin auf Ausgang
#elif defined (__AVR_ATmega8__)
          DDRD |= (1 << ONEWIREPIN);    // Pin auf Ausgang
#endif    
          status = PRESENCEPULSE;
          bitcount = 0;
          TCNT0 = ~PRESENCEZEIT;     // Neu Starten zum bestimmen der Presencepulselaenge
          TIFR0 |= (1 << TOV0);
          TIMSK0 |= (1 << TOIE0);       // Timer Interrupt an
    }
    else if (status == PRESENCEPULSE){
#ifdef __AVR_ATtiny13__
          DDRB &= ~(1 << ONEWIREPIN);    // Pin auf Eingang
#elif defined (__AVR_ATmega8__)
          DDRD &= ~(1 << ONEWIREPIN);    // Pin auf Eingang
#endif    
          status = RECEIVE_OPCODE;
          bitcount = 0;
    }
    else if (status & (RECEIVE_OPCODE | MATCHROM | WRITEMEM)){
	    bitcount++;
    	    if (bitcount == 1) {
		transbyte = 0;
	    }
            transbyte = transbyte >> 1;
#ifdef __AVR_ATtiny13__
            if (PINB & (1 << ONEWIREPIN)){
#elif defined (__AVR_ATmega8__)
            if (PIND & (1 << ONEWIREPIN)){
#endif    
		transbyte |= 0x080;
	    } 
            if (bitcount == 8){
		if (status == RECEIVE_OPCODE) {
                        if (transbyte == 0x55){
                              status = MATCHROM;
                              transbyte = 0;    // New                        
	   		}
                     	else if (transbyte == 0x4E){
                              status = WRITEMEM;
                              transbyte = 0;
                     	}
                     	else if (transbyte == 0x48){  // Write data to EEPROM
                            for (tim0_i = 2; tim0_i < 4; tim0_i++){
                                while(EECR & (1<<EEPE)) ;
				/* Set Programming mode */
				//EECR = (0<<EEPM1)|(0>>EEPM0);
				/* Set up address and data registers */
				EEARL = tim0_i;            // EEPROM Address
				EEDR = transdata[tim0_i];
				/* Write logical one to EEMPE */
				EECR |= (1<<EEMPE);
				/* Start eeprom write by setting EEPE */
				EECR |= (1<<EEPE);
			    }
			      status = IDLE;
                              transbyte = 0;
                     	}
                        else if (transbyte == 0xBE){
        //        PORTD |= (1 << PAD_RELAIS);    // Debug Relais  an 
                              status = READMEM;
                              shift_reg = 0;     // Schiebergister fÃ¼r CRC loeschen
			     transbyte = transdata[0];
                        }
			else {
				status = IDLE;
			}
			bitcount = 0;
			bytecount = 0;
		}
		else if (status == MATCHROM){           
         		if (bytecount == 0){
                          	if (transbyte == FAMILYCODE);
	        		else status = IDLE;
                     	} 
       		      	else if (bytecount < 7){
       				if (transbyte ==  DEVICEID) ;
				else  status = IDLE;
		    	}
               	    	else {                  // Byte 8
				status = RECEIVE_OPCODE; // Eigentlich CRC checken, aber wozu ?
		      	}
               	    	if (status == IDLE){ 
		      		bytecount = 0;
               	    	}
                    	bytecount++;
		    	bitcount = 0;
              	}	
		else if (status == WRITEMEM){           
         		if (bytecount == 0){
                          	transdata[2] = transbyte;
                     	} 
       		      	else {
       				transdata[3] = transbyte;
				status = RECEIVE_OPCODE;
				bytecount = 0;
		    	}
                    	bytecount++;
		    	bitcount = 0;
              	}	
    	  }
    }
//    else if (status == READMEM){
//              DDRB &= ~(1 << ONEWIREPIN);    // Pin auf Eingang
//    }
    else {
#ifdef __AVR_ATtiny13__
          DDRB &= ~(1 << ONEWIREPIN);    // Pin auf Eingang
#elif defined (__AVR_ATmega8__)
          DDRD &= ~(1 << ONEWIREPIN);    // Pin auf Eingang
#endif    
    }
    //TCNT0 = 0;                  // Neu Starten
    status_global = status;
}


// Flankenerkennung am 1-Wire pin, entsprechend wird dann dei Aktion ausgewählt
ISR (INT0_vect)
{
    unsigned char tim0_i, status;
    status = status_global;
//      PORTD &= ~(1 << PAD_RELAIS);    // Debuggen Relais aus
#ifdef __AVR_ATtiny13__
      if (PINB & (1 << ONEWIREPIN)){     // Steigende Flanke am One Wire
#elif defined (__AVR_ATmega8__)
      if (PIND & (1 << ONEWIREPIN)){     // Steigende Flanke am One Wire
#endif    
            if (((TCNT0 < 0xF0)||(status == IDLE)) && (TCNT0 > 50)) {    // Reset pulse erkannt, unschoen ?
                  TIFR0 |= (1 << TOV0);
                  TCNT0 = ~PRESENCEWAITZEIT;                  // Timer Neu Starten Fuer Presencepulse
                  TIMSK0 |= (1 << TOIE0);       // Timer Interrupt an
                  status = RESET;
            }
      }
      else {                                      // Fallende Flanke am One Wire
      //PORTB &= ~(1 << PAD_RELAIS);    // Debug Relais aus
          TIFR0 |= (1 << TOV0);
          if (status == READMEM){
                if ((transbyte & 0x01) == 0){
#ifdef __AVR_ATtiny13__
                      DDRB |= (1 << ONEWIREPIN);    // Pin auf Ausgang
#elif defined (__AVR_ATmega8__)
                      DDRD |= (1 << ONEWIREPIN);    // Pin auf Ausgang
#endif    
                  }
                TCNT0  = ~SENDEZEIT;
                TIMSK0 |= (1 << TOIE0);       // Timer Interrupt an
         	fb_bit = (transbyte ^ shift_reg) & 0x01;
         	shift_reg = shift_reg >> 1;
         	if (fb_bit)
           		shift_reg = shift_reg ^ 0x8c;
                bitcount++;
                transbyte = transbyte >> 1;
                if (bitcount == 8){
                        bitcount = 0;
                        bytecount++;
                        if (bytecount == 8){
				 transbyte = shift_reg;  // CRC senden
                        }
                        else if (bytecount == 9) status = IDLE;  // CRC senden
			else transbyte = transdata[bytecount];
                }
          }
          else if (status == IDLE) {           // Erste fallende Flanke
              TIMSK0 &= ~(1 << TOIE0);       // Timer Interrupt aus
              TCNT0 = 0;
              status = RESET;
          }
          else if (status & (RECEIVE_OPCODE | MATCHROM | WRITEMEM)){
                  TCNT0 = ~ABTASTZEIT;          // Zeichen abtasten in ABTASTZEIT
                  TIMSK0 |= (1 << TOIE0);       // Timer Interrupt an
          }
          else if (status == RESET) {           // Da hat ein anderer einen Presence Pulse gesendet
              status = RECEIVE_OPCODE;
              TIMSK0 &= ~(1 << TOIE0);       // Timer Interrupt aus
          }
	  else if (status == PRESENCEPULSE);   // Gar nichts tun, Pegeländerung kommt vom eigenen Timer beim Senden
	  else {
		status = IDLE;
                TCNT0 = 0;
                TIMSK0 &= ~(1 << TOIE0);       // Timer Interrupt aus
     	  }	
     }
    status_global = status;
}
