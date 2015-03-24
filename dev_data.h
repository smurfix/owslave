#ifndef dev_data
#define dev_data

#include <inttypes.h>

#define EEPROM_POS 128
#define EUID_LEN 8

// mirrored in gen_eeprom
typedef enum _ConfigID {
	CfgID_euid     = 1,
	CfgID_rf12     = 2,
	CfgID_crypto   = 3,
	CfgID_owid     = 4,
} ConfigID;

#define CFG_DATA(n) struct config_##n
#define cfg_read(n,x) _cfg_read(&x, sizeof(struct config_##n), CfgID_##n)
#ifdef CFG_EEPROM
#define cfg_write(n,x) _cfg_write(&x, sizeof(struct config_##n), CfgID_##n)
#endif

char _cfg_read(void *data, uint8_t size, ConfigID id);
#ifdef CFG_EEPROM
char _cfg_write(void *data, uint8_t size, ConfigID id);
#endif

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

#endif // dev_data
