void shift(uint8_t out);
void latch(uint8_t out);
void keys_write_line(uint8_t l);
void keys_next_line();
void keys_write();
void flash_write_line(uint8_t l);
void flash_next_line();
void flash_write();

uint8_t keys_current_line = 0;
uint8_t keys[8] = {
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111111,
	0b11111111
};

uint8_t flash_current_line = 0;
uint8_t flash[3] = {
	0b11111111,
	0b11111111,
	0b11111111
};