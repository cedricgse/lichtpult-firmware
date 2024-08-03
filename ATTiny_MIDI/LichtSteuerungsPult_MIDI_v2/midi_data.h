#ifndef midi_data_h
#define midi_data_h

#define BUFFER_SIZE_M 64

//uint8_t buffer[BUFFER_SIZE_M];
//uint8_t buffer_pos;
//uint8_t buffer_read_pos;

uint8_t ovf_counter;

void send_action(char c);

#endif