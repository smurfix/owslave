#!/usr/bin/make -f
# Makefile
# initially written by guido socher
# extended by Matthias Urlichs

CFG?=moat.cfg

ifeq ($(DEV),)

TARGET?=$(shell ./cfg ${CFG} targets)
help: 
	@echo "Usage: make all | DEV | burn_DEV"
	@echo "Known devices: $$(./cfg ${CFG} .devs)"
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
	
clean:
	rm -r device

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
ifeq (shell ./cfg ${CFG} devices.${DEV}.defs.use_eeprom,1)
EEP:=-U eeprom:w:device/${DEV}/eprom.bin:i
else
EEP:=
endif
burn: all
	TF=$$(tempfile); echo "default_safemode = no;" >$$TF; \
	sudo avrdude -c $(PROG) -p $(MCU_PROG) -C +$$TF \
		-U flash:w:device/${DEV}/image.hex:i ${EEP} \
		-U lfuse:w:0x$(shell ./cfg ${CFG} devices.${DEV}.fuse.l):m \
		-U hfuse:w:0x$(shell ./cfg ${CFG} devices.${DEV}.fuse.h):m \
		-U efuse:w:0x$(shell ./cfg ${CFG} devices.${DEV}.fuse.e):m \
	; X=$$?; rm $$TF; exit $$X
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
	./gen_eeprom $@ type $$(./cfg ${CFG} .type ${DEV}); \
	if [ ${OW_TYPE} != 0 ] ; then \
		if ./gen_eeprom $@ owid serial >/dev/null 2>&1 ; then \
			SER=$$(./gen_eeprom $@ owid serial); \
			if ./cfg ${CFG} devices.${DEV}.onewire_id >/dev/null 2>&1 ; then \
				test $$(./cfg ${CFG} devices.${DEV}.onewire_id) -eq $$SER ; \
			else \
				./cfg_write ${CFG} devices.${DEV}.onewire_id $$SER; \
			fi; \
		else \
			./gen_eeprom $@ owid type 0x$$(./cfg ${CFG} codes.onewire.${OW_TYPE}) serial random; \
			./cfg_write ${CFG} devices.${DEV}.onewire_id $$(./gen_eeprom $@ owid serial); \
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

clean:
	rm -r device/${DEV}

endif
