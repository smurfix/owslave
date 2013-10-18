# makefile, written by guido socher
#MCU=atmega8
#MCU=attiny13
MCU=attiny84
#MCU=attiny4313
#MCU=atmega168
#MCU_PROG=m168
MCU_PROG=t84
#MCU_PROG=attiny4313
#PROG=usbtiny
PROG=avrispmkII
PORT=usb

ARCH=avr
CC=$(ARCH)-gcc
OBJCOPY=$(ARCH)-objcopy
OBJDUMP=$(ARCH)-objdump

#-------------------
help: 
	@echo "Usage: make TYPE | TYPE_burn"
	@echo "Known Types: ds2408 ds2409 ds2423"

#-------------------

# device codes
ds2408_CODE=29
ds2409_CODE=1F
ds2423_CODE=1D

all: $(DEVNAME).hex $(DEVNAME).lss $(DEVNAME).bin

ds2408 ds2409 ds2423:
	 @make $@_dev

%_burn: %_dev
	@make DEVNAME=$(subst _burn,,$@) DEVCODE=$($(subst _burn,,$@)_CODE) burn

%_dev:
	@make DEVNAME=$(subst _dev,,$@) all

# optimize for size!
ifeq ($(ARCH),avr)
  CFLAGS=-g -mmcu=$(MCU) -Wall -Wstrict-prototypes -Os -mcall-prologues
  UART=avr_uart.o
else
  CFLAGS=-g -mcpu=cortex-m0 -mthumb -Wall -Wstrict-prototypes -Os
  UART=cortexm0_uart.o
endif
#  -I/usr/local/avr/include -B/usr/local/avr/lib
#-------------------
%.o : %.c Makefile $(wildcard *.h)
	$(CC) $(CFLAGS) -c $<
$(DEVNAME).out : onewire.o $(DEVNAME).o $(UART)
	$(CC) $(CFLAGS) -o $@ -Wl,-Map,$(DEVNAME).map,--cref $^
$(DEVNAME).hex : $(DEVNAME).out 
	$(OBJCOPY) -R .eeprom -O ihex $< $@
$(DEVNAME).lss : $(DEVNAME).out 
	$(OBJDUMP) -h -S $< > $@
$(DEVNAME).bin : $(DEVNAME).out 
	$(OBJCOPY) -O binary $< $@

$(DEVNAME).eeprom:
	python gen_eeprom.py $(DEVCODE) > $@
#------------------
burn: $(DEVNAME).hex $(DEVNAME).eeprom
	avrdude -V -c $(PROG) -p $(MCU_PROG) -P $(PORT) -U flash:w:$(DEVNAME).hex:i -U eeprom:w:$(DEVNAME).eeprom:i 
	#avrdude -V -c $(PROG) -p $(MCU_PROG) -U $(PRG).bin
#-------------------
fuse: 
	avrdude -c $(PROG) -p $(MCU_PROG) -P $(PORT) -U lfuse:w:0xCF:m  -U hfuse:w:0xDF:m -U efuse:w:0xff:m
#0xEE
#-------------------
clean:
	rm -f *.o *.map *.out *.hex *.bin *.lss
#-------------------

