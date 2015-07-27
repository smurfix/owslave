static void dummy_init_fn(void) {}
static void dummy_poll_fn(void) {}
static uint8_t dummy_read_len_fn(uint8_t chan) { next_idle('y'); return 0; }
static void dummy_read_fn(uint8_t chan, uint8_t *buf) { next_idle('y'); }
static void dummy_read_done_fn(uint8_t chan) {}
static void dummy_write_check_fn(uint8_t chan, uint8_t *buf, uint8_t len) { next_idle('y'); }
static void dummy_write_fn(uint8_t chan, uint8_t *buf, uint8_t len) { next_idle('y'); }
static char dummy_alert_check_fn(void) { return 0; }
static void dummy_alert_fill_fn(uint8_t *buf) { next_idle('y'); }
