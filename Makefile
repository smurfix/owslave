#!/usr/bin/make -f
# Makefile
# initially written by guido socher
# extended by Matthias Urlichs

CFG?=moat.cfg

ifeq ($(DEV),)

TARGET?=$(shell ./cfg ${CFG} targets)
all: setup $(addprefix run/,${TARGET})
setup: device
device:
	mkdir device
run/%:
	@make DEV=$(notdir $@) NO_BURN=y

targets:
	./cfg ${CFG} targets

burn_%:
	@echo BURN $(subst burn_,,$@)
	@make DEV=$(subst burn_,,$@) burn
%:
	@make DEV=$@ all
	
.PHONY: all setup targets


else # DEV is defined

MCU:=$(shell ./cfg ${CFG} devices.${DEV}.mcu)
MCU_PROG:=$(shell ./cfg ${CFG} devices.${DEV}.prog)
PROG:=$(shell ./cfg ${CFG} env.prog)

CC=avr-gcc
OBJCOPY:=avr-objcopy
OBJDUMP:=avr-objdump
CFLAGS:=-g -mmcu=$(MCU) -Wall -Wstrict-prototypes -Os -mcall-prologues -fshort-enums
LDFLAGS:=$(CFLAGS)
#CFLAGS+=$(shell ./cfg ${CFG} .cdefs ${DEV})
CFLAGS+=-Idevice/${DEV}

OW_TYPE:=$(shell ./cfg ${CFG} devices.${DEV}.defs.is_onewire || echo 0)

OBJS:=$(addprefix device/${DEV}/,$(addsuffix .o,$(basename $(shell ./cfg ${CFG} .cfiles ${DEV}))))

ifeq (${NO_BURN},)
burn: all
	sudo avrdude -c $(PROG) -p $(MCU_PROG) \
		-U lfuse:w:0x$(shell ./cfg ${CFG} devices.${DEV}.fuse.l):m \
		-U hfuse:w:0x$(shell ./cfg ${CFG} devices.${DEV}.fuse.h):m \
		-U efuse:w:0x$(shell ./cfg ${CFG} devices.${DEV}.fuse.e):m \
		-U flash:w:device/${DEV}/image.hex:i 
		-U eeprom:w:device/${DEV}/eprom.bin:i 
endif

all: device/${DEV} device/${DEV}/image.hex device/${DEV}/eprom.bin device/${DEV}/image.lss
device/${DEV}: 
	mkdir $@
device/${DEV}/image.hex: device/${DEV}/image.elf
	$(OBJCOPY) -R .eeprom -O ihex $< $@
device/${DEV}/image.elf: ${OBJS}
	$(CC) $(LDFLAGS) -o $@ -Wl,-Map,device/${DEV}/image.map,--cref $^
device/${DEV}/image.lss: device/${DEV}/image.elf
	$(OBJDUMP) -h -S $< > $@

device/${DEV}/dev_config.h: ${CFG}
	./cfg ${CFG} .hdr ${DEV}

$(DEVNAME).hex : $(DEVNAME).elf
device/${DEV}/eprom.bin: ${CFG}
	set -e; \
	if [ ${OW_TYPE} != 0 ] ; then \
		./gen_eeprom $@ type $$(./cfg ${CFG} .type ${DEV}); \
		if ./gen_eeprom $@ owid serial >/dev/null 2>&1 ; then \
			SER=$$(./gen_eeprom $@ owid serial); \
			if ./cfg ${CFG} devices.${DEV}.onewire_id >/dev/null 2>&1 ; then \
				test $$(./cfg ${CFG} devices.${DEV}.onewire_id) -eq $$SER ; \
			else \
				./cfgw ${CFG} devices.${DEV}.onewire_id $$SER; \
			fi; \
		else \
			./gen_eeprom $@ owid type 0x$$(./cfg ${CFG} codes.onewire.${OW_TYPE}) serial random; \
			./cfgw ${CFG} devices.${DEV}.onewire_id $$(./gen_eeprom $@ owid serial); \
		fi; \
	fi

device/${DEV}/config.o: device/${DEV}/eprom.bin
	${OBJCOPY} -I binary -O elf32-avr --prefix-sections=.progmem \
		--redefine-sym "_binary_device_${DEV}_eprom_bin_start=_config_start" \
		--redefine-sym "_binary_device_${DEV}_eprom_bin_size=_config_size" \
		--redefine-sym "_binary_device_${DEV}_eprom_bin_end=_config_end" \
		$^ $@

device/${DEV}/%.o: %.c device/${DEV}/dev_config.h
	$(CC) $(CFLAGS) -c -o $@ $<

	

#MCU=atmega8
#MCU=attiny13
#MCU=attiny84
#MCU=atmega168
MCU?=atmega88
#MCU=attiny25
MCU_PROG?=m88
#MCU_PROG=m168
#MCU_PROG=t84


#-------------------
help: 
	@echo "Usage: make TYPE | TYPE_burn"
	@echo "Known Types: ds2408 ds2423"

#-------------------

# device codes
ds2408_CODE=29
ds2423_CODE=1D

DEVNAME?=ds2408
dev: $(DEVNAME).hex $(DEVNAME).lss $(DEVNAME).bin

ds2408 ds2423:
	 @make $@_dev

%_burn: device/%
	@make DEVNAME=$(subst _burn,,$@) DEVCODE=$($(subst _burn,,$@)_CODE) burn

%_dev:
	@make DEVNAME=$(subst _dev,,$@) all

# optimize for size:

#  -I/usr/local/avr/include -B/usr/local/avr/lib
#-------------------
%.o : %.c Makefile $(wildcard *.h)
	$(CC) $(CFLAGS) -Os -c $<
$(DEVNAME).elf : onewire.o uart.o $(DEVNAME).o irq_catcher.o
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
_burn: $(DEVNAME).hex $(DEVNAME).eeprom
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
fsb: fastslave.hex
	sudo avrdude -c $(PROG) -p $(MCU_PROG) -U flash:w:fastslave.hex:i
endif
