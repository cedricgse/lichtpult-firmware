/*
 * keyboard_data.h
 *
 * Created: 21.09.2021 12:14:46
 *  Author: Cedric
 */ 


#ifndef KEYBOARD_DATA_H_
#define KEYBOARD_DATA_H_

#define ROW_COUNT 8
#define COLUMN_COUNT 8

//Fix "Bug" in Lighting software, where next, last fixture selection scrolls through pages
//tradeoff: if FIX_PAGEUP is defined as 1, last fixture selection will always scroll to page 2, if window is on page 1
#define FIX_PAGEDOWN 1
#define FIX_PAGEUP 0

#define NEED_PAGEDOWN 1
#define NEED_PAGEUP 2

uint8_t keys[ROW_COUNT][COLUMN_COUNT];
uint8_t pageUpDownState = 0; // 1 = PageDown needed, 2 = PageUp needed
//uint8_t buffer_read_pos;

void send_action(char c);
void build_keys_report();
void build_pageUpDown_report(); //Used to fix Last/Next fixture select scrolling through pages

#endif /* KEYBOARD_DATA_H_ */