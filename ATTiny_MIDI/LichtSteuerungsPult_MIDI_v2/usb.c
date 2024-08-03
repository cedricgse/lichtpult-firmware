/*
 * 4chord MIDI - USB MIDI
 *
 * Copyright (C) 2020 Sven Gregori <sven@craplab.fi>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://www.gnu.org/licenses/
 *
 */
#include <string.h>
#include <stdint.h>
#include <util/delay.h>
#include "usbconfig.h"
#include "usbdrv.h"

//static const char send_failed_string[] PROGMEM = "USB send failed\r\n";




/**
 * Generic USB MIDI message send function.
 * USB MIDI messages are always 4 byte long (padding unused bytes with zero).
 *
 * See also chapter 4 in the Universal Serial Bus Device Class Definition
 * for MIDI Devices Release 1.0 document found at
 * https://usb.org/sites/default/files/midi10.pdf
 *
 * For more information on MIDI messages, refer to Summary of MIDI Messages,
 * found at https://www.midi.org/specifications/item/table-1-summary-of-midi-message
 *
 * @param byte0 USB cable number and code index number
 * @param byte1 MIDI message status byte
 * @param byte2 MIDI message data byte 0
 * @param byte3 MIDI message data byte 1
 */
void
usb_send_midi_message(uint8_t byte0, uint8_t byte1, uint8_t byte2, uint8_t byte3)
{
    static uint8_t midi_buf[4];
    uint8_t retries = 10;

    while (--retries) {
        if (usbInterruptIsReady()) {
            midi_buf[0] = byte0;
            midi_buf[1] = byte1;
            midi_buf[2] = byte2;
            midi_buf[3] = byte3;
            usbSetInterrupt(midi_buf, 4);
            return;
        }
        _delay_ms(2);
    }
}

