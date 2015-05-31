#ifndef dev_data_h
#define dev_data_h

#include <inttypes.h>
#include "dev_config.h"

#define EUID_LEN 8

#define CFG_DATA(n) struct config_##n
#define cfg_read(n,x) _cfg_read(&x, sizeof(struct config_##n), CfgID_##n)
#ifdef CFG_EEPROM
#define cfg_write(n,x) _cfg_write(&x, sizeof(struct config_##n), CfgID_##n)
#endif

char _cfg_read(void *data, uint8_t size, ConfigID id);
#ifdef CFG_EEPROM
char _cfg_write(void *data, uint8_t size, ConfigID id);
#endif

#ifdef SMALL_DEVICE
typedef uint8_t cfg_addr_t;
#else
typedef uint16_t cfg_addr_t;
#endif

void cfg_addr(cfg_addr_t *addr, uint8_t *size, ConfigID id);
uint8_t cfg_byte(cfg_addr_t addr);

/* enumerate config entries. Zero == no more. */
uint8_t cfg_count(cfg_addr_t *addr);
uint8_t cfg_type(cfg_addr_t *addr);

struct config_rf12 { // for radio devices
	unsigned int band:2;
	unsigned int collect:1; // monitor mode: don't ack
	unsigned int node:5;
	uint8_t group;   // RF12 sync pattern
	uint8_t speed;   // bitrate for RF12
};

struct config_euid {
	uint8_t id[EUID_LEN];
};

struct config_crypto {
	uint32_t key[4];
};

struct config_owid {
	uint8_t type;
	uint8_t serial[6];
	uint8_t crc;
};

// data for M_INFO; see OWFS:ow_moat.h for details
struct config_info {
	uint8_t types;
	uint8_t data[0];
};

#endif // dev_data_h
