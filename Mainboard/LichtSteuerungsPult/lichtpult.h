#define TX_BUFFERSIZE 128

#define FADER_COUNT 10
#define FADER_MEASUREMENTS 16

#define I2C_BUFFERSIZE 16

#define DMX_OVF 7

//DEBUG
void print_keys();
void print_flash();
void print_fader();

//KEYS
char keys[8] = {0, 0, 0, 0, 0, 0, 0, 0};
uint8_t keys_read_pos = 0;
void keys_write();
void keys_shift();
void keys_read_line();
void keys_read_shift();
void keys_read();
void keys_load();
uint8_t keys_send(uint8_t l, uint8_t c, uint8_t s);

//FLASH
char flashs[3] = {0, 0, 0};
uint8_t flash_state = 0;
uint8_t flash_read_pos = 0;
void flash_shift();
void flash_read_line();
void flash_read_shift();
void flash_read();
void flash_load();
uint8_t flash_send(uint8_t l, uint8_t c, uint8_t s);

//FADER
uint8_t faders[FADER_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
uint8_t fader_raw[FADER_COUNT][FADER_MEASUREMENTS];
uint8_t fader_state = 255;
uint8_t fader_write_pos = 0;
void fader_shift();
void fader_read_value(uint8_t pos, uint8_t init);
void fader_average();
void fader_read(uint8_t init);
//void fader_send(uint8_t chan, uint8_t val);

//DMX
void dmx_send();
uint8_t dmx_timer_ovf = 0;

//UART BUFFER
uint8_t tx_write_pos = 0;
uint8_t tx_read_pos = 0;
char tx_buffer[TX_BUFFERSIZE];
void write_buffer(char c);

//TWI
unsigned char i2c_write_buffer(char address);
char i2c_buffer[I2C_BUFFERSIZE];
uint8_t i2c_buffer_pos = 0;