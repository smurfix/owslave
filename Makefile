# makefile, written by guido socher
#MCU=atmega8
MCU=attiny13
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
$(FILENAME).o : $(FILENAME).c 
	$(CC) $(CFLAGS) -Os -c $(FILENAME).c
$(FILENAME).out : $(FILENAME).o  
	$(CC) $(CFLAGS) -o $(FILENAME).out -Wl,-Map,$(FILENAME).map,--cref $(FILENAME).o 
$(FILENAME).hex : $(FILENAME).out 
	$(OBJCOPY) -R .eeprom -O ihex $(FILENAME).out $(FILENAME).hex 
$(FILENAME).lss : $(FILENAME).out 
	$(OBJDUMP) -h -S $(FILENAME).out  > $(FILENAME).lss
$(FILENAME).bin : $(FILENAME).out 
	$(OBJCOPY) -O binary $(FILENAME).out $(FILENAME).bin 
#------------------
load: $(FILENAME).hex
	avrdude -p t13 -c Olimex -U flash:w:$(FILENAME).hex -E noreset -v 
# here is a pre-compiled version in case you have trouble with
# your development environment
load_pre: $(FILENAME).hex
	./prg_load_uc $(FILENAME).hex
#
loaduisp: $(FILENAME).hex
	./prg_load_uc -u $(FILENAME).hex
# here is a pre-compiled version in case you have trouble with
# your development environment
load_preuisp: $(FILENAME).hex
	./prg_load_uc -u $(FILENAME).hex
#-------------------
# fuse byte settings:
#  Atmel AVR ATmega8 
#  Fuse Low Byte      = 0xe1 (1MHz internal), 0xe3 (4MHz internal), 0xe4 (8MHz internal)
#  Fuse High Byte     = 0xd9 
#  Factory default is 0xe1 for low byte and 0xd9 for high byte
# Check this with make rdfuses
rdfuses:
	./prg_fusebit_uc -r
# use internal RC oscillator 1 Mhz
wrfuse1mhz:
	./prg_fusebit_uc -w 1
# use internal RC oscillator 4 Mhz
wrfuse4mhz:
	./prg_fusebit_uc -w 4
# use external 3-8 Mhz crystal
# Warning: you can not reset this to intenal unless you connect a crystal!!
wrfusecrystal:
	@echo "Warning: The external crystal setting can not be changed back without a working crystal"
	@echo "         You have 3 seconds to abort this with crtl-c"
	@sleep 3
	./prg_fusebit_uc -w 0
#-------------------
clean:
	rm -f *.o *.map *.out *t.hex
#-------------------

