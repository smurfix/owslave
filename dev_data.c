
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <util/crc16.h>

#include "dev_data.h"
#include "debug.h"

#define CRC 0  // needs 300 bytes
#define DEFAULT 1 // needs 30 bytes, plus _config data (built by gen_eprom)

#ifdef USE_EEPROM
extern uint8_t _econfig_start;
#define EEPROM_POS (&_econfig_start)
#else
extern uint8_t _config_start;
#endif

/*
 * Layout of configuration blocks:
 *  4 signature 'DevC'
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
    return pgm_read_byte(_config_start+addr);
#endif
}
#define read_byte(x) cfg_byte(x)

#if CRC
static uint8_t read_crc_byte(uint16_t &crc, uint8_t pos) {
	uint8_t b = read_byte((uint8_t *)EEPROM_POS + pos);
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

#if USE_EEPROM
char _do_crc(bool update) // from eeprom; True if CRC matches
{
	static bool crc_checked = false;
	static bool crc_good = false;

	if(!update && crc_checked)
		return crc_good;

	uint16_t crc = ~0;
	uint8_t b, i=0, j;

	for(j=0;j<4;j++) {
		if(read_crc_byte(crc, i++) != read_byte(j)) {
			return false;
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
		crc_good = true;
	} else {
		read_crc_byte(crc, i++);
		read_crc_byte(crc, i++);
		crc_good = (crc == 0);
	}
	crc_checked = true;
	return crc_good;
}
#else // !crc
#define _do_crc(x) true
#endif



char _cfg_read(void *data, uint8_t size, ConfigID id) {
	cfg_addr_t off;
	uint8_t len;
	uint8_t *d = data;

	cfg_addr(&off,&len,id);
	if((!off) || (size != len)) return 0;

	while(len) {
		*d++ = read_byte(off++);
		len--;
	}
	return 1;
}

#ifdef CFG_EEPROM
char _cfg_write(void *addr, uint8_t size, ConfigID id) {
}
#endif
    
void cfg_addr(cfg_addr_t *addr, uint8_t *size, ConfigID id) {
	cfg_addr_t i=4;
	uint8_t len,t;
	while((len = read_byte(i++)) > 0) {
		t = read_byte(i++);
		if (t == id) {
			*addr = i;
			*size = len;
			return;
		} else
			i += len;
	}
	*addr = 0;
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

