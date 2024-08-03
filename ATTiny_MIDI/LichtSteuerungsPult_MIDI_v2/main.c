/*
 * main.c
 *
 * Created: 9/18/2021 10:39:26 PM
 *  Author: Cedric
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <compat/twi.h>
#include "midi_data.h"

#include "usbdrv.h"
#include "oddebug.h"
#include "usb.h"

#include "usiTwiSlave.h"

#define ADDRESS_THIS 0x10
#define F_SCL 100000
#define PORT_SCL PB2
#define PORT_SDA PB0

#define NONE				0
#define ACK_PR_RX			1
#define BYTE_RX				2
#define ACK_PR_TX			3
#define PR_ACK_TX			4
#define BYTE_TX				5


const uint8_t address_decode[3][8] = {
	{50, 51, 52, 53, 54, 55, 56, 57},
	{60, 61, 62, 63, 64, 65, 66, 67},
	{73, 72, 71, 70, 69, 68, 59, 58},
};


// This descriptor is based on http://www.usb.org/developers/devclass_docs/midi10.pdf
//
// Appendix B. Example: Simple MIDI Adapter (Informative)
// B.1 Device Descriptor
//
const static PROGMEM char deviceDescrMIDI[] = {	/* USB device descriptor */
	18,			/* sizeof(usbDescriptorDevice): length of descriptor in bytes */
	USBDESCR_DEVICE,	/* descriptor type */
	0x10, 0x01,		/* USB version supported */
	0,			/* device class: defined at interface level */
	0,			/* subclass */
	0,			/* protocol */
	8,			/* max packet size */
	USB_CFG_VENDOR_ID,	/* 2 bytes */
	USB_CFG_DEVICE_ID,	/* 2 bytes */
	USB_CFG_DEVICE_VERSION,	/* 2 bytes */
	1,			/* manufacturer string index */
	2,			/* product string index */
	0,			/* serial number string index */
	1,			/* number of configurations */
};

// B.2 Configuration Descriptor
const static PROGMEM char configDescrMIDI[] = {	/* USB configuration descriptor */
	9,			/* sizeof(usbDescrConfig): length of descriptor in bytes */
	USBDESCR_CONFIG,	/* descriptor type */
	101, 0,			/* total length of data returned (including inlined descriptors) */
	2,			/* number of interfaces in this configuration */
	1,			/* index of this configuration */
	0,			/* configuration name string index */
	#if USB_CFG_IS_SELF_POWERED
	USBATTR_SELFPOWER,	/* attributes */
	#else
	USBATTR_BUSPOWER,	/* attributes */
	#endif
	USB_CFG_MAX_BUS_POWER / 2,	/* max USB current in 2mA units */

	// B.3 AudioControl Interface Descriptors
	// The AudioControl interface describes the device structure (audio function topology)
	// and is used to manipulate the Audio Controls. This device has no audio function
	// incorporated. However, the AudioControl interface is mandatory and therefore both
	// the standard AC interface descriptor and the classspecific AC interface descriptor
	// must be present. The class-specific AC interface descriptor only contains the header
	// descriptor.

	// B.3.1 Standard AC Interface Descriptor
	// The AudioControl interface has no dedicated endpoints associated with it. It uses the
	// default pipe (endpoint 0) for all communication purposes. Class-specific AudioControl
	// Requests are sent using the default pipe. There is no Status Interrupt endpoint provided.
	/* AC interface descriptor follows inline: */
	9,			/* sizeof(usbDescrInterface): length of descriptor in bytes */
	USBDESCR_INTERFACE,	/* descriptor type */
	0,			/* index of this interface */
	0,			/* alternate setting for this interface */
	0,			/* endpoints excl 0: number of endpoint descriptors to follow */
	1,			/* */
	1,			/* */
	0,			/* */
	0,			/* string index for interface */

	// B.3.2 Class-specific AC Interface Descriptor
	// The Class-specific AC interface descriptor is always headed by a Header descriptor
	// that contains general information about the AudioControl interface. It contains all
	// the pointers needed to describe the Audio Interface Collection, associated with the
	// described audio function. Only the Header descriptor is present in this device
	// because it does not contain any audio functionality as such.
	/* AC Class-Specific descriptor */
	9,			/* sizeof(usbDescrCDC_HeaderFn): length of descriptor in bytes */
	36,			/* descriptor type */
	1,			/* header functional descriptor */
	0x0, 0x01,		/* bcdADC */
	9, 0,			/* wTotalLength */
	1,			/* */
	1,			/* */

	// B.4 MIDIStreaming Interface Descriptors

	// B.4.1 Standard MS Interface Descriptor
	/* interface descriptor follows inline: */
	9,			/* length of descriptor in bytes */
	USBDESCR_INTERFACE,	/* descriptor type */
	1,			/* index of this interface */
	0,			/* alternate setting for this interface */
	2,			/* endpoints excl 0: number of endpoint descriptors to follow */
	1,			/* AUDIO */
	3,			/* MS */
	0,			/* unused */
	0,			/* string index for interface */

	// B.4.2 Class-specific MS Interface Descriptor
	/* MS Class-Specific descriptor */
	7,			/* length of descriptor in bytes */
	36,			/* descriptor type */
	1,			/* header functional descriptor */
	0x0, 0x01,		/* bcdADC */
	65, 0,			/* wTotalLength */

	// B.4.3 MIDI IN Jack Descriptor
	6,			/* bLength */
	36,			/* descriptor type */
	2,			/* MIDI_IN_JACK desc subtype */
	1,			/* EMBEDDED bJackType */
	1,			/* bJackID */
	0,			/* iJack */

	6,			/* bLength */
	36,			/* descriptor type */
	2,			/* MIDI_IN_JACK desc subtype */
	2,			/* EXTERNAL bJackType */
	2,			/* bJackID */
	0,			/* iJack */

	//B.4.4 MIDI OUT Jack Descriptor
	9,			/* length of descriptor in bytes */
	36,			/* descriptor type */
	3,			/* MIDI_OUT_JACK descriptor */
	1,			/* EMBEDDED bJackType */
	3,			/* bJackID */
	1,			/* No of input pins */
	2,			/* BaSourceID */
	1,			/* BaSourcePin */
	0,			/* iJack */

	9,			/* bLength of descriptor in bytes */
	36,			/* bDescriptorType */
	3,			/* MIDI_OUT_JACK bDescriptorSubtype */
	2,			/* EXTERNAL bJackType */
	4,			/* bJackID */
	1,			/* bNrInputPins */
	1,			/* baSourceID (0) */
	1,			/* baSourcePin (0) */
	0,			/* iJack */


	// B.5 Bulk OUT Endpoint Descriptors

	//B.5.1 Standard Bulk OUT Endpoint Descriptor
	9,			/* bLenght */
	USBDESCR_ENDPOINT,	/* bDescriptorType = endpoint */
	0x1,			/* bEndpointAddress OUT endpoint number 1 */
	3,			/* bmAttributes: 2:Bulk, 3:Interrupt endpoint */
	8, 0,			/* wMaxPacketSize */
	10,			/* bIntervall in ms */
	0,			/* bRefresh */
	0,			/* bSyncAddress */

	// B.5.2 Class-specific MS Bulk OUT Endpoint Descriptor
	5,			/* bLength of descriptor in bytes */
	37,			/* bDescriptorType */
	1,			/* bDescriptorSubtype */
	1,			/* bNumEmbMIDIJack  */
	1,			/* baAssocJackID (0) */


	//B.6 Bulk IN Endpoint Descriptors

	//B.6.1 Standard Bulk IN Endpoint Descriptor
	9,			/* bLenght */
	USBDESCR_ENDPOINT,	/* bDescriptorType = endpoint */
	0x81,			/* bEndpointAddress IN endpoint number 1 */
	3,			/* bmAttributes: 2: Bulk, 3: Interrupt endpoint */
	8, 0,			/* wMaxPacketSize */
	10,			/* bIntervall in ms */
	0,			/* bRefresh */
	0,			/* bSyncAddress */

	// B.6.2 Class-specific MS Bulk IN Endpoint Descriptor
	5,			/* bLength of descriptor in bytes */
	37,			/* bDescriptorType */
	1,			/* bDescriptorSubtype */
	1,			/* bNumEmbMIDIJack (0) */
	3,			/* baAssocJackID (0) */
};


uchar usbFunctionDescriptor(usbRequest_t * rq)
{

	if (rq->wValue.bytes[1] == USBDESCR_DEVICE) {
		usbMsgPtr = (uchar *) deviceDescrMIDI;
		return sizeof(deviceDescrMIDI);
		} else {		/* must be config descriptor */
		usbMsgPtr = (uchar *) configDescrMIDI;
		return sizeof(configDescrMIDI);
	}
}


static uchar sendEmptyFrame;


/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

uchar usbFunctionSetup(uchar data[8])
{
	usbRequest_t *rq = (void *) data;

	if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {	/* class request type */

		/*  Prepare bulk-in endpoint to respond to early termination   */
		if ((rq->bmRequestType & USBRQ_DIR_MASK) == USBRQ_DIR_HOST_TO_DEVICE) sendEmptyFrame = 1;
	}

	return 0xff;
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

volatile uint8_t COMM_STATUS = NONE;
static uchar sendEmptyFrame;


int main(void)
{	
	//TCCR0B |= (0b101 << CS02);
	
	wdt_enable(WDTO_1S);
	
	odDebugInit();
	usbInit();
	#ifdef USB_CFG_PULLUP_IOPORT	/* use usbDeviceConnect()/usbDeviceDisconnect() if available */
	USBDDR = 0;		/* we do RESET by deactivating pullup */
	usbDeviceDisconnect();
	#else
	USBDDR = (1 << USB_CFG_DMINUS_BIT) | (1 << USB_CFG_DPLUS_BIT);
	#endif
	for(uint8_t i = 0; i<250; i++) { // wait 500 ms
		wdt_reset(); // keep the watchdog happy
		_delay_ms(2);
	}
	usiTwiSlaveInit(ADDRESS_THIS);
	sendEmptyFrame = 0;
	#ifdef USB_CFG_PULLUP_IOPORT
	usbDeviceConnect();
	#else
	USBDDR = 0;		/*  remove USB reset condition */
	#endif
	
	//TWI
	usiTwiSlaveInit(ADDRESS_THIS);
	
	sei();
	
    while(1)
    { 
		wdt_reset();
		usbPoll();
		if(usiTwiDataInReceiveBuffer() && usbInterruptIsReady())
		{
			send_action(usiTwiReceiveByte());
		}
    }
}

void send_action(char c)
{
	uint8_t row = (c & (0b11100000)) >> 5;
	uint8_t column = (c & (0b00011100)) >> 2;
	uint8_t status = (c & (0b00000010)) >> 1;
	
	if(row >= 3 || column >= 8 || status > 1) return;
	
	//uchar message[4];
	
	if(status) //If Key Pressed
	{
		midi_msg_note_on(address_decode[row][column], 0x7f);
		/*
		message[0] = 0x09;
		message[1] = 0x90;
		message[2] = address_decode[row][column];
		message[3] = 0x7f;
		*/
	}
	else
	{
		midi_msg_note_off(address_decode[row][column], 0x00);
		/*
		message[0] = 0x08;
		message[1] = 0x80;
		message[2] = address_decode[row][column];
		message[3] = 0x00;
		*/
	}
	sendEmptyFrame = 0;
	//usbSetInterrupt(message, 4);
}