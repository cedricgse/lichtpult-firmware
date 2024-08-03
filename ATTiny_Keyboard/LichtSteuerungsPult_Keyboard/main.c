/*
 * LichtSteuerungsPult_Keyboard.c
 *
 * Created: 21.09.2021 12:00:11
 * Author : Cedric
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <compat/twi.h>

#include <stdio.h>
#include <string.h>

#include "usbdrv.h"
#include "oddebug.h"

#include "keyboard_data.h"
#include "usb_hid_keys.h"
#include "usitwislave.h"


#define ADDRESS_THIS 0x12

const uint8_t address_decode[8][8] PROGMEM = {
	{KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8},
	{KEY_F9, KEY_F10, KEY_F11, KEY_ESC, KEY_P, 0, KEY_H, KEY_PAGEUP},
	{KEY_PAGEDOWN, KEY_S, KEY_X, KEY_O, KEY_Z, KEY_B, KEY_F, KEY_D},
	{KEY_J, KEY_K, KEY_F12, KEY_HOME, KEY_END, KEY_TAB, KEY_SPACE, 0},
	{KEY_E, KEY_N, KEY_DELETE, KEY_C, KEY_M, KEY_DELETE, 0, 0},
	{KEY_KPSLASH, KEY_KPMINUS, KEY_KPPLUS, KEY_BACKSPACE, KEY_KP7, KEY_KP8, KEY_KP9, KEY_T},
	{KEY_KP4, KEY_KP5, KEY_KP6, KEY_I, KEY_KP1, KEY_KP2, KEY_KP3, KEY_A},
	{KEY_KP0, KEY_DOT, KEY_KPENTER, KEY_R, KEY_U, KEY_L, KEY_G, KEY_Q}
};

const uint8_t address_ctrl[8][8] PROGMEM = {
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 1, 0, 1, 1},
	{1, 1, 1, 1, 1, 1, 1, 1},
	{1, 1, 0, 1, 1, 1, 1, 0},
	{1 ,1, 1, 1, 1, 1, 0, 0},
	{0 ,0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 1, 0, 0, 0, 1},
	{0, 0, 0, 1, 1, 1, 1, 1}
};

/* The ReportBuffer contains the USB report sent to the PC */
static uchar reportBuffer[8];    /* buffer for HID reports */
static uchar idleRate;           /* in 4 ms units */
static uchar protocolVer=1;      /* 0 is the boot protocol, 1 is report protocol */

/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

/* USB report descriptor (length is defined in usbconfig.h)
   This has been changed to conform to the USB keyboard boot
   protocol */

const char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] 
  PROGMEM = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
    0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x03,                    //   REPORT_SIZE (3)
    0x91, 0x03,                    //   OUTPUT (Cnst,Var,Abs)
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x65,                    //   LOGICAL_MAXIMUM (101) 0x65
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application) 0x65
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0                           // END_COLLECTION  
};

uchar usbFunctionSetup(uchar data[8]) {
	usbRequest_t *rq = (void *)data;
	usbMsgPtr = reportBuffer;
	if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */
		if(rq->bRequest == USBRQ_HID_GET_REPORT){
			/* wValue: ReportType (highbyte), ReportID (lowbyte) */
			/* we only have one report type, so don't look at wValue */
			return sizeof(reportBuffer);
			}else if(rq->bRequest == USBRQ_HID_SET_REPORT){
			if (rq->wLength.word == 1) { /* We expect one byte reports */
				return 0xFF; /* Call usbFunctionWrite with data */
			}
			}else if(rq->bRequest == USBRQ_HID_GET_IDLE){
			usbMsgPtr = &idleRate;
			return 1;
			}else if(rq->bRequest == USBRQ_HID_SET_IDLE){
			idleRate = rq->wValue.bytes[1];
			}else if(rq->bRequest == USBRQ_HID_GET_PROTOCOL) {
			if (rq->wValue.bytes[1] < 1) {
				protocolVer = rq->wValue.bytes[1];
			}
			}else if(rq->bRequest == USBRQ_HID_SET_PROTOCOL) {
			usbMsgPtr = &protocolVer;
			return 1;
		}
	}
	return 0;
}


#define abs(x) ((x) > 0 ? (x) : (-x))

// Called by V-USB after device reset
void hadUsbReset() {
	int frameLength, targetLength = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5);
	int bestDeviation = 9999;
	uchar trialCal, bestCal, step, region;

	// do a binary search in regions 0-127 and 128-255 to get optimum OSCCAL
	for(region = 0; region <= 1; region++) {
		frameLength = 0;
		trialCal = (region == 0) ? 0 : 128;
		
		for(step = 64; step > 0; step >>= 1) {
			if(frameLength < targetLength) // true for initial iteration
			trialCal += step; // frequency too low
			else
			trialCal -= step; // frequency too high
			
			OSCCAL = trialCal;
			frameLength = usbMeasureFrameLength();
			
			if(abs(frameLength-targetLength) < bestDeviation) {
				bestCal = trialCal; // new optimum found
				bestDeviation = abs(frameLength -targetLength);
			}
		}
	}

	OSCCAL = bestCal;
}

int main(void)
{	
	uchar updateNeeded = 0;
	uchar idleCounter = 0;
	memset(keys, 0, sizeof(keys));
	
	/* configure timer 0 for a rate of 12M/(1024 * 256) = 45.78 Hz (~22ms) */
	TCCR0B |= 5;      /* timer 0 prescaler: 1024 */
	
	wdt_enable(WDTO_1S);
	usbDeviceDisconnect();
	for(uint8_t i = 0; i < 20; i++){  /* 300 ms disconnect */
		_delay_ms(15);
	}
	usbDeviceConnect();
	odDebugInit();
	usbInit();
	
	//TWI
	usiTwiSlaveInit(ADDRESS_THIS);
	
	
	sei();
	
    while(1)
    { 
		wdt_reset();
		usbPoll();
		if(usiTwiDataInReceiveBuffer())
		{
			send_action(usiTwiReceiveByte());
			build_keys_report();
			updateNeeded = 1;
		}else if(pageUpDownState != 0) //pageUpDown only "bug"fix for lighting software, so priority is lower
		{
			build_pageUpDown_report();
			updateNeeded = 1;
		}
		
		/* Check timer if we need periodic reports */
		if(TIFR & (1<<TOV0)){
			TIFR |= 1<<TOV0; /* Reset flag */
			if(idleRate != 0){ /* Do we need periodic reports? */
				if(idleCounter > 4){ /* Yes, but not yet */
					idleCounter -= 5;   /* 22 ms in units of 4 ms */
					}else{ /* Yes, it is time now */
					updateNeeded = 1;
					idleCounter = idleRate;
				}
			}
		}
		
		/* If an update is needed, send the report */
		if(updateNeeded && usbInterruptIsReady()){
			updateNeeded = 0;
			usbSetInterrupt(reportBuffer, sizeof(reportBuffer));
		}
    }
}


void send_action(char c)
{
	uint8_t row = (c & (0b11100000)) >> 5;
	uint8_t column = (c & (0b00011100)) >> 2;
	uint8_t status = (c & (0b00000010)) >> 1;
	
	if(status)
	{
		keys[row][column] = 1;
	}
	else
	{
		keys[row][column] = 0;
	}
}

void build_keys_report()
{
	uint8_t key = 0;
	uint8_t num_key = 2;
	
	memset(reportBuffer,0,sizeof(reportBuffer)); /* Clear report buffer */
	for(uint8_t row = 0; row < ROW_COUNT; row++)
	{
		for(uint8_t column = 0; column < COLUMN_COUNT; column++)
		{
			if(keys[row][column])
			{
				key=pgm_read_byte(&address_decode[row][column]);  /* Read keyboard map */
				if(key == KEY_PAGEUP && FIX_PAGEUP) pageUpDownState = NEED_PAGEDOWN;
				if(key == KEY_PAGEDOWN && FIX_PAGEDOWN) pageUpDownState = NEED_PAGEUP;
				if(key == 0) continue;
				if(pgm_read_byte(&address_ctrl[row][column]))/* Is this a modifier key? */
				{
					reportBuffer[0]|=KEY_MOD_LCTRL;
				}
				if(row == 4 && column == 5)
				{
					reportBuffer[0] |= KEY_MOD_LSHIFT;
				}
				if(num_key >= sizeof(reportBuffer))
				{
					memset(reportBuffer+2, KEY_ERR_OVF, sizeof(reportBuffer)-2);
					return;
				}
				reportBuffer[num_key]=key; /* Set next available entry */
				num_key++;
			}
		}
	}
}

void build_pageUpDown_report()
{
	memset(reportBuffer,0,sizeof(reportBuffer)); /* Clear report buffer */
	if(pageUpDownState == NEED_PAGEUP)
	{
		reportBuffer[2] = KEY_PAGEUP;
	}else if(pageUpDownState == NEED_PAGEDOWN)
	{
		reportBuffer[2] = KEY_PAGEDOWN;
	}
	pageUpDownState = 0;
}