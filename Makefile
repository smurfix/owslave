# makefile, written by guido socher
#MCU=atmega8
#MCU=attiny13
#MCU=attiny84
#MCU=atmega168
MCU=attiny25
MCU_PROG=m168
#MCU_PROG=t84
PROG=usbtiny

CC=avr-gcc
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump

#-------------------
help: 
	@echo "Usage: make TYPE | TYPE_burn"
	@echo "Known Types: ds2408 ds2423"

#-------------------

# device codes
ds2408_CODE=29
ds2423_CODE=1D

DEVNAME=ds2408
all: $(DEVNAME).hex $(DEVNAME).lss $(DEVNAME).bin

ds2408 ds2423:
	 @make $@_dev

%_burn: %_dev
	@make DEVNAME=$(subst _burn,,$@) DEVCODE=$($(subst _burn,,$@)_CODE) burn

%_dev:
	@make DEVNAME=$(subst _dev,,$@) all

# optimize for size:
CFLAGS=-g -mmcu=$(MCU) -Wall -Wstrict-prototypes -Os -mcall-prologues -fshort-enums

#  -I/usr/local/avr/include -B/usr/local/avr/lib
#-------------------
%.o : %.c Makefile $(wildcard *.h)
	$(CC) $(CFLAGS) -Os -c $<
$(DEVNAME).elf : onewire.o uart.o $(DEVNAME).o
	$(CC) $(CFLAGS) -o $@ -Wl,-Map,$(DEVNAME).map,--cref $^
$(DEVNAME).hex : $(DEVNAME).elf
	$(OBJCOPY) -R .eeprom -O ihex $< $@
$(DEVNAME).lss : $(DEVNAME).elf
	$(OBJDUMP) -h -S $< > $@
$(DEVNAME).bin : $(DEVNAME).elf
	$(OBJCOPY) -O binary $< $@

$(DEVNAME).eeprom:
	python gen_eeprom.py $(DEVCODE) > $@
#------------------
burn: $(DEVNAME).hex $(DEVNAME).eeprom
	#sudo avrdude -c $(PROG) -p $(MCU_PROG) -U flash:w:$(DEVNAME).hex:i -U eeprom:w:$(DEVNAME).eeprom:i 
	sudo avrdude -c $(PROG) -p $(MCU_PROG) -U flash:w:$(DEVNAME).hex:i
	#avrdude -V -c $(PROG) -p $(MCU_PROG) -U $(PRG).bin
#-------------------
clean:
	rm -f *.o *.map *.elf *t.hex
#-------------------

fs: fastslave.hex
fastslave.hex: fastslave.c
	avr-gcc $(CFLAGS) -c fastslave.c -o fastslave.o
	avr-gcc $(CFLAGS) fastslave.o -o fastslave.elf
	avr-objdump -h -S fastslave.elf > fastslave.lss
	avr-objcopy -O ihex  fastslave.elf fastslave.hex
	
