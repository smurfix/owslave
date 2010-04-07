# makefile, written by guido socher
#MCU=atmega8
#MCU=attiny13
MCU=atmega168
MCU_PROG=m168
PROG=usbtiny

CC=avr-gcc
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
FILENAME=onewire
# optimize for size:
CFLAGS=-g -mmcu=$(MCU) -Wall -Wstrict-prototypes -Os -mcall-prologues -I/usr/local/avr/include -B/usr/local/avr/lib
#-------------------
all: $(FILENAME).hex $(FILENAME).lss $(FILENAME).bin
#-------------------
help: 
	@echo "Usage: make all|load|load_pre|rdfuses|wrfuse1mhz|wrfuse4mhz|wrfusecrystal"
	@echo "Warning: you will not be able to undo wrfusecrystal unless you connect an"
	@echo "         external crystal! uC is dead after wrfusecrystal if you do not"
	@echo "         have an external crystal."
#-------------------
$(FILENAME).o : $(FILENAME).c Makefile
	$(CC) $(CFLAGS) -Os -c $(FILENAME).c
$(FILENAME).out : $(FILENAME).o # uart.o
	$(CC) $(CFLAGS) -o $(FILENAME).out -Wl,-Map,$(FILENAME).map,--cref $^
$(FILENAME).hex : $(FILENAME).out 
	$(OBJCOPY) -R .eeprom -O ihex $(FILENAME).out $(FILENAME).hex 
$(FILENAME).lss : $(FILENAME).out 
	$(OBJDUMP) -h -S $(FILENAME).out  > $(FILENAME).lss
$(FILENAME).bin : $(FILENAME).out 
	$(OBJCOPY) -O binary $(FILENAME).out $(FILENAME).bin 

uart.o: uart.c
	$(CC) $(CFLAGS) -Os -c uart.c
#------------------
burn: $(FILENAME).hex
	avrdude -c $(PROG) -p $(MCU_PROG) -U flash:w:$(FILENAME).hex -E noreset -v 
	#avrdude -V -c $(PROG) -p $(MCU_PROG) -U $(PRG).bin
#-------------------
clean:
	rm -f *.o *.map *.out *t.hex
#-------------------

