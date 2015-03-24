
#include <avr/eeprom.h>
#include <util/crc16.h>
#include <avr/pgmspace.h>

#include "dev_data.h"

#define CRC 0  // needs 300 bytes
#define DEFAULT 1 // needs 30 bytes, plus _config data (built by DeviceID.py)

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
 *
 */

char _cfg_read(void *data, uint8_t size, ConfigID id);
#ifdef CFG_EEPROM
char _cfg_write(void *data, uint8_t size, ConfigID id);
using namespace DeviceConfig;
    
#if CRC
static uint8_t read_byte(uint16_t &crc, uint8_t pos) {
	uint8_t b = eeprom_read_byte((uint8_t *)EEPROM_POS + pos);
	crc = _crc16_update(crc, b);
	return b;
}
#define read_crc_byte(x,y) read_byte(x,y)
#else
#define read_crc_byte(x,y) read_byte(y)
#endif

static inline uint8_t read_byte(uint8_t pos) {
	return eeprom_read_byte((uint8_t *)EEPROM_POS + pos);
}

static inline void write_byte(uint8_t b, uint8_t pos) {
	eeprom_write_byte((uint8_t *)EEPROM_POS + pos, b);
}

#if DEFAULT
extern "C" {
extern const unsigned char _config_start[] __attribute__ ((progmem));
extern const unsigned char _config_end[] __attribute__ ((progmem));
}

static inline void copy_prog()
{
	const unsigned char *config = _config_start;
	uint8_t i;
	while(config < _config_end)
		write_byte(i++, pgm_read_byte(config++));
}
#else
const unsigned char _config_start[] PROGMEM = "DevC";
#define copy_prog() return false

#endif

#if CRC
bool DeviceConfig::_do_crc(bool update) // from eeprom; True if CRC matches
{
	static bool crc_checked = false;
	static bool crc_good = false;

	if(!update && crc_checked)
		return crc_good;

	uint16_t crc = ~0;
	uint8_t b, i=0, j;

	for(j=0;j<4;j++) {
		if(read_crc_byte(crc, i++) != pgm_read_byte(_config_start+j)) {
			copy_prog();
			break;
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


bool DeviceConfig::_do_cf(ConfigID id, void *data, uint8_t size, bool write = false)
{
	uint8_t i=4, j;
#if CRC
	if(!_do_crc(false))
		return false;
#else
	for(j=0;j<4;j++) {
		if(read_crc_byte(crc, i++) != pgm_read_byte(_config_start+j)) {
			copy_prog();
			break;
		}
	}
#endif

	while ((j = read_byte(i++)) > 0) {
		if (j != size) {
			i += j+1;
			continue;
		}
		if (read_byte(i++) != id) {
			i += j;
			continue;
		}
		uint8_t *dt = (uint8_t *)data;
		while(j--) {
			if (write)
				write_byte(*dt++, i++);
			else
				*dt++ = read_byte(i++);
		}
		if(write)
			_do_crc(true);
		return true;
	}
	return false;
}

namespace DeviceConfig {
template<> bool read_cf(config_rf12 &x) { return _read_cf(CfgID_rf12, (void *)&x, sizeof(x)); }
template<> bool write_cf(config_rf12 &x) { return _write_cf(CfgID_rf12, (void *)&x, sizeof(x)); }

template<> bool read_cf(config_euid &x) { return _read_cf(CfgID_euid, (void *)&x, sizeof(x)); }
template<> bool write_cf(config_euid &x) { return _write_cf(CfgID_euid, (void *)&x, sizeof(x)); }

template<> bool read_cf(config_crypto &x) { return _read_cf(CfgID_crypto, (void *)&x, sizeof(x)); }
template<> bool write_cf(config_crypto &x) { return _write_cf(CfgID_crypto, (void *)&x, sizeof(x)); }

}
