
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <util/crc16.h>

#include "dev_data.h"
#include "debug.h"

#define CRC 0  // needs 300 bytes
#define DEFAULT 1 // needs 30 bytes, plus _config data (built by gen_eprom)

#ifndef USE_EEPROM
#define EEPROM_VALID 1
#else
static char eep = 0;
#define EEPROM_VALID eep
#endif

#if USE_EEPROM == 1
extern uint8_t _econfig_start;
#define EEPROM_POS (&_econfig_start)
#elif USE_EEPROM == 2
uint8_t _econfig_start[100] __attribute__((section(".eeprom")));
#define EEPROM_POS (_econfig_start)
#endif
extern uint8_t _config_start;
extern uint8_t _config_end;

/*
 * Layout of configuration blocks:
 *  4 signature 'MoaT'
 *  repeat:
 *    1 length =n >0
 *    1 type
 *    n struct (whatever)
 *  1 zero byte (delimiter)
 *  2 CRC
 *
 *  Struct sizes must match exactly.
 */

inline uint8_t cfg_byte(cfg_addr_t addr) {
#ifdef USE_EEPROM
    return eeprom_read_byte(EEPROM_POS+addr);
#else
    return pgm_read_byte((&_config_start)+addr);
#endif
}
#define read_byte(x) cfg_byte(x)

#if 0 /* def USE_EEPROM_CRC */
static uint8_t read_crc_byte(uint16_t &crc, uint8_t pos) {
	uint8_t b = read_byte((uint8_t *)pos);
	crc = _crc16_update(crc, b);
	return b;
}
#else
#define read_crc_byte(x,y) read_byte(y)
#endif

#ifdef USE_EEPROM
static inline void write_byte(uint8_t b, uint8_t pos) {
	eeprom_write_byte((uint8_t *)EEPROM_POS + pos, b);
}
#endif

#ifdef USE_EEPROM_CRC
char _do_crc(char update) // from eeprom; True if CRC matches
{
	static char crc_checked = 0;
	static char crc_good = 0;

	if(!update && crc_checked)
		return crc_good;

	uint16_t crc = ~0;
	uint8_t i=0, j;

	for(j=0;j<4;j++) {
		if(read_crc_byte(crc, i++) != read_byte(j)) {
			return 0;
		}
	}

	while ((j = read_crc_byte(crc, i++)) > 0) {
		read_crc_byte(crc, i++); // type
		while(j--)
			read_crc_byte(crc, i++); // data
	}

	if(update) {
		write_byte(i++, crc & 0xFF);
		write_byte(i++, crc >> 8);
		crc_good = 1;
	} else {
		read_crc_byte(crc, i++);
		read_crc_byte(crc, i++);
		crc_good = (crc == 0);
	}
	crc_checked = 1;
	return crc_good;
}
#else // !crc
#define _do_crc(x) 1
#endif

#ifdef USE_EEPROM
void eeprom_init(void)
{
    if (eeprom_read_byte(EEPROM_POS+0) == 'M' &&
        eeprom_read_byte(EEPROM_POS+1) == 'o' &&
        eeprom_read_byte(EEPROM_POS+2) == 'a' &&
        eeprom_read_byte(EEPROM_POS+3) == 'T') {
		eep = _do_crc(0);
		if (eep) return;
	}
#if USE_EEPROM == 2
	{
		uint8_t *cp = &_config_start;
		uint8_t *ep = EEPROM_POS;
		while(cp != &_config_end) {
			eeprom_write_byte(ep, pgm_read_byte(cp));
			cp++; ep++;
		}
		eep = 1;
		return;
	}
#else
	eep = 0;
#endif
}
#endif

char _cfg_read(void *data, uint8_t size, ConfigID id) {
	cfg_addr_t off;
	uint8_t len;
	uint8_t *d = data;

	off = cfg_addr(&len,id);
	if((!off) || (size != len)) return 0;

	while(len) {
		*d++ = read_byte(off++);
		len--;
	}
	return 1;
}

#ifdef USE_EEPROM
inline cfg_addr_t cfg_addr_w(uint8_t size, ConfigID id) {
	cfg_addr_t off;
	uint8_t sz;

	if(!EEPROM_VALID)
		return 0;
	// Look for the existing block
	off = cfg_addr (&sz, id);
	if (off != 0) {
		// return if sizes match
		if (sz == size)
			return off;
		// otherwise mark as free
		write_byte(--off, 0);
		if (size == 0)
			return 0;
	}
	off=4;
	while((sz = read_byte(off++)) > 0) {
		ConfigID cur_id = read_byte(off++);
		if (sz == size && cur_id == 0) {
			// found a free block
			write_byte(off-1, id);
			return off;
		}
		off += sz;
	}
	// No free space. Add to the end.
	write_byte(off-1,size);
	write_byte(off++,id);
	write_byte(off+size,0);
	return off;
}

char _cfg_write(void *data, uint8_t size, ConfigID id) {
	cfg_addr_t off;
	if (!EEPROM_VALID)
		return 0;
	off = cfg_addr_w(size,id);
	if (size) {
		if (!off)
			return 0;
		do {
			write_byte(off++, *(uint8_t *)data++);
		} while (--size);
	}
	return _do_crc(1);
}
#endif
    
cfg_addr_t cfg_addr(uint8_t *size, ConfigID id) {
	cfg_addr_t off=4;
	uint8_t len,t;
	if (!EEPROM_VALID)
		return 0;

	while((len = read_byte(off++)) > 0) {
		t = read_byte(off++);
		if (t == id) {
			*size = len;
			return off;
		}
		off += len;
	}
	return 0;
}

uint8_t cfg_count(cfg_addr_t *addr) {
	cfg_addr_t pos = *addr = 4;
	uint8_t res = 0;
	uint8_t len;

	while((len = read_byte(pos)) > 0) {
		pos += len+2;
		res += 1;
	}
	return res;
}

uint8_t cfg_type(cfg_addr_t *addr) {
	cfg_addr_t pos = *addr;
	uint8_t len, t;

	len = read_byte(pos++);
	if (!len) return 0;
	t = read_byte(pos++);
	*addr = pos+len;
	return t;
}

