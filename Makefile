#!/usr/bin/make -f
# Makefile
# initially written by guido socher
# extended by Matthias Urlichs

SHELL=/bin/bash
CFG?=world.cfg
export CFG

RUN_CFG?=./cfg
RUN_CFGWRITE?=./cfg_write
RUN_EEPROM?=./gen_eeprom
RUN_ELF_END?=./elf_end
LD:=avr-ld

ifeq ($(DEV),)

TARGET?=$(shell $(RUN_CFG) ${CFG} targets)
help burn: 
	@echo "Usage: make all | DEV | burn_DEV"
	@echo "Known devices: $$($(RUN_CFG) ${CFG} .devs)"
all: setup $(addprefix run/,${TARGET})
setup: device
device:
	mkdir device
run/%:
	@$(MAKE) DEV=$(notdir $@) NO_BURN=y

targets:
	$(RUN_CFG) ${CFG} targets

burn_%:
	@echo BURN $(subst burn_,,$@)
	@$(MAKE) DEV=$(subst burn_,,$@) burn
%:
	@$(MAKE) DEV=$@ all

clean:
	rm -r device

test:
	$(MAKE) test_ds2423
	$(MAKE) test_ds2408
	$(MAKE) -q test8 || $(MAKE) burn_test8 || true
	./run_test

.PHONY: all setup targets


else # DEV is defined

MCU:=$(shell $(RUN_CFG) ${CFG} devices.${DEV}.mcu)
MCU_PROG:=$(shell $(RUN_CFG) ${CFG} devices.${DEV}.prog)
PROG:=$(shell $(RUN_CFG) ${CFG} env.prog)
AVRDUDE:=$(shell $(RUN_CFG) ${CFG} env.avrdude)

CC=avr-gcc
OBJCOPY:=avr-objcopy
OBJDUMP:=avr-objdump
CFLAGS:=-g -mmcu=$(MCU) -Wall -Wstrict-prototypes -Os -mcall-prologues -fshort-enums
LDFLAGS:=
#CFLAGS+=$(shell $(RUN_CFG) ${CFG} .cdefs ${DEV})
CFLAGS+=-Idevice/${DEV}

OW_TYPE:=$(shell $(RUN_CFG) ${CFG} devices.${DEV}.defs.is_onewire || echo 0)

OBJS:=$(addprefix device/${DEV}/,$(addsuffix .o,$(basename $(shell $(RUN_CFG) ${CFG} .cfiles ${DEV}))))

ifeq ($(shell $(RUN_CFG) ${CFG} devices.${DEV}.defs.use_eeprom),1)
EE:=device/${DEV}/eprom.hex
EEP:=-U eeprom:w:$(EE):i
PROM=eeprom
PSYM=econfig
else
EE:=
EEP:=
PROM=progmem
PSYM=config
endif

ifeq ($(shell $(RUN_CFG) ${CFG} devices.${DEV}.defs.is_bootloader || echo 0),1)
# Build a boot loader. This needs to use the SPM instruction, which must be
# located in high memory. Thus boot.c gets its own code block.
EE+=device/${DEV}/boot.hex
EEP+=-U flash:w:device/${DEV}/boot.hex:i
BOOTADR=$(shell printf "0x%x" $$(( $(shell $(RUN_CFG) ${CFG} devices.${DEV}.flash.size) * 1024 - 128)) )
BOOTLDFLAGS:=-Wl,--defsym=boot_program_page=${BOOTADR}
endif

LOADER:=$(shell $(RUN_CFG) ${CFG} devices.${DEV}.defs.use_bootloader || echo 0)
ifeq ($(LOADER),0)
# build "traditional" code

ifeq (${NO_BURN},)
burn_cfg:
	@echo -n LFUSE:
	@$(RUN_CFG) ${CFG} devices.${DEV}.fuse.l
	@echo -n HFUSE:
	@$(RUN_CFG) ${CFG} devices.${DEV}.fuse.h
	@echo -n EFUSE:
	@$(RUN_CFG) ${CFG} devices.${DEV}.fuse.e
	@echo -n EEPROM:
	@$(RUN_CFG) ${CFG} devices.${DEV}.defs.use_eeprom

burn: burn_cfg all $(EE)
	TF=$$(mktemp avrdude-cfg.XXXXX); echo "default_safemode = no;" >$$TF; \
	EFUSE=$(shell $(RUN_CFG) ${CFG} devices.${DEV}.fuse.e); \
	[ "${EFUSE}" != "" ] && SET_EFUSE="-U efuse:w:0x$(shell $(RUN_CFG) ${CFG} devices.${DEV}.fuse.e):m"; \
	$(AVRDUDE) -c $(PROG) -p $(MCU_PROG) -C +$$TF\
		-U flash:w:device/${DEV}/image.hex:i ${EEP} \
		-U lfuse:w:0x$(shell $(RUN_CFG) ${CFG} devices.${DEV}.fuse.l):m \
		-U hfuse:w:0x$(shell $(RUN_CFG) ${CFG} devices.${DEV}.fuse.h):m \
		${SET_EFUSE} \
	; X=$$?; rm $$TF; exit $$X
endif # NO_BURN

all: device/${DEV}/cfg device/${DEV}/image.hex device/${DEV}/eprom.hex device/${DEV}/image.lss ${EE}

device/${DEV}/image.elf: ${OBJS}
	$(CC) $(BOOTLDFLAGS) $(CFLAGS) -o $@ -Wl,-Map,device/${DEV}/image.map,--cref $^

device/${DEV}/boot.elf: device/${DEV}/image.elf device/${DEV}/boot.o module.ld
	$(LD) $(LDFLAGS) -o $@ -T module.ld \
		--section-start=.mtext=${BOOTADR} \
		-Map device/${DEV}/boot.map --cref device/${DEV}/boot.o \
		-R device/${DEV}/image.elf 
	test $$(( $$($(RUN_ELF_END) device/${DEV}/boot.elf mtext) )) -lt $$(( $$($(RUN_CFG) ${CFG} devices.${DEV}.flash.size) * 1024 ))
device/${DEV}/boot.hex: device/${DEV}/boot.elf
	$(OBJCOPY) -R mtext -O ihex $< $@

else
# Build dependant code, using $LOADER as the base.

all: device/${DEV}/cfg device/${DEV}/image.bin device/${DEV}/eprom.bin device/${DEV}/image.lss

device/${LOADER}/image.elf:
	@echo You did no yet build the $(LOADER) base image. Exiting.
	@false

#		--defsym=mtext_start=$$TS 
device/${DEV}/image.elf: device/${LOADER}/image.elf ${OBJS} module.ld
	$(LD) $(LDFLAGS) -o $@ -T module.ld \
		--section-start=.mtext=$(shell $(RUN_ELF_END) device/${LOADER}/image.elf text $(shell $(RUN_CFG) ${CFG} devices.${DEV}.flash.align)) \
		--section-start=.mdata=$(shell $(RUN_ELF_END) device/${LOADER}/image.elf bss 2) \
		-Map device/${DEV}/image.map --cref $(OBJS) \
		-R device/${LOADER}/image.elf 
	test $$(( $(shell $(RUN_ELF_END) device/${DEV}/image.elf mtext) )) -lt $$(( $(shell $(RUN_CFG) ${CFG} devices.${DEV}.flash.size) * 1024 ))
device/${DEV}/image.bin: device/${DEV}/image.elf
	$(OBJCOPY) -R mtext -O binary $< $@

burn: all
	ow_send $(shell $(RUN_EEPROM) device/${DEV}/eprom.bin owid ascii) device/${DEV}/image.bin device/${DEV}/eprom.bin

endif

device/${DEV}: 
	mkdir -p $@
device/${DEV}/cfg: device/${DEV} ${CFG} cfg
	@echo -n MCU:
	@$(RUN_CFG) ${CFG} devices.${DEV}.mcu
	@echo -n MCU_PROG:
	@$(RUN_CFG) ${CFG} devices.${DEV}.prog
	@echo -n PROG:
	@$(RUN_CFG) ${CFG} env.prog
	@echo -n AVRDUDE:
	@$(RUN_CFG) ${CFG} env.avrdude
	@echo -n CFILES:
	@$(RUN_CFG) ${CFG} .cfiles ${DEV}
	@echo -n TYPE:
	@$(RUN_CFG) ${CFG} .type ${DEV}
	@cp ${CFG} device/${DEV}/cfg
device/${DEV}/image.hex: device/${DEV}/image.elf
	$(OBJCOPY) -R .eeprom -O ihex $< $@
device/${DEV}/image.lss: device/${DEV}/image.elf
	$(OBJDUMP) -h -S $< > $@
device/${DEV}/eprom.hex: device/${DEV}/image.elf
	$(OBJCOPY) -j .eeprom --change-section-address .eeprom=0 -O ihex $< $@

device/${DEV}/dev_config.h: ${CFG} cfg
	mkdir -p device/${DEV}
	$(RUN_CFG) ${CFG} .hdr ${DEV}

$(DEVNAME).hex : $(DEVNAME).elf
device/${DEV}/eprom.bin: device/${DEV} ${CFG}
	set -e; \
	$(RUN_EEPROM) $@ type $$($(RUN_CFG) ${CFG} .type ${DEV}); \
	if $(RUN_EEPROM) $@ name >/dev/null 2>&1 ; then : ; else \
		$(RUN_EEPROM) $@ name ${DEV} ; fi ; \
	if [ ${OW_TYPE} != 0 ] ; then \
		if $(RUN_EEPROM) $@ owid serial >/dev/null 2>&1 ; then \
			SER=$$($(RUN_EEPROM) $@ owid serial); \
			if $(RUN_CFG) ${CFG} devices.${DEV}.onewire_id >/dev/null 2>&1 ; then \
				test "$$($(RUN_CFG) ${CFG} devices.${DEV}.onewire_id)" = "$$SER" ; \
			else \
				$(RUN_CFGWRITE) ${CFG} devices.${DEV}.onewire_id x$$SER; \
			fi; \
		elif $(RUN_CFG) ${CFG} .nofollow devices.${DEV}.onewire_id >/dev/null 2>&1 ; then \
			SER=$$($(RUN_CFG) ${CFG} devices.${DEV}.onewire_id); \
			$(RUN_EEPROM) $@ owid type 0x$$($(RUN_CFG) ${CFG} codes.onewire.${OW_TYPE}) serial $$SER; \
		else \
			$(RUN_EEPROM) $@ owid type 0x$$($(RUN_CFG) ${CFG} codes.onewire.${OW_TYPE}) serial random; \
			$(RUN_CFGWRITE) ${CFG} devices.${DEV}.onewire_id x$$($(RUN_EEPROM) $@ owid serial); \
		fi; \
	fi

device/${DEV}/owadr.bin: device/${DEV}/eprom.bin
	$(RUN_EEPROM) $^ owid binary > $@

#device/${DEV}/owadr.o: device/${DEV}/owadr.bin
#	${OBJCOPY} -I binary -O elf32-avr --prefix-sections=.eeprom \
#		--redefine-sym "_binary_device_${DEV}_owadr_bin_start=_owadr_start" \
#		--redefine-sym "_binary_device_${DEV}_owadr_bin_size=_owadr_size" \
#		--redefine-sym "_binary_device_${DEV}_owadr_bin_end=_owadr_end" \
#		$^ $@

device/${DEV}/config.o: device/${DEV}/eprom.bin
	${OBJCOPY} -I binary -O elf32-avr --prefix-sections=.$(PROM) \
		--redefine-sym "_binary_device_${DEV}_eprom_bin_start=_$(PSYM)_start" \
		--redefine-sym "_binary_device_${DEV}_eprom_bin_size=_$(PSYM)_size" \
		--redefine-sym "_binary_device_${DEV}_eprom_bin_end=_$(PSYM)_end" \
		$^ $@

device/${DEV}/%.o: %.c device/${DEV}/dev_config.h *.h
	$(CC) $(CFLAGS) -c -o $@ $<
device/${DEV}/%.o: %.S device/${DEV}/dev_config.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -r device/${DEV}

.PHONY: burn_cfg
endif
