#define F_CPU 16000000UL

#include <xc.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>
#include <compat/twi.h>
#include "lichtpult.h"
#include <util/twi.h>
#include <avr/wdt.h>
#include <string.h>

#define C_DELAY 5

#define DMX_BAUD 250000UL
#define F_SCL 100000UL
#define DMX_TIMER_OVF_COUNT 2

#define A_MIDI 0x10
#define A_HDI 0x12

//DDR has to be set manually in main()
#define KEYS PORTD
#define KEYS_IN PIND
#define KEYS_SER_OUT PORTD2
#define KEYS_SCLK PORTD3
#define KEYS_RCLK PORTD4
#define KEYS_SER_IN PIND5
#define KEYS_SHLD PORTD6
#define KEYS_CLK PORTD7 //Clock Input Shift Register

//DDR has to be set manually in main()
#define FLASH PORTB
#define FLASH_IN PINB
#define FLASH_OUT_0 PORTB3
#define FLASH_OUT_1 PORTB4
#define FLASH_OUT_2 PORTB5
#define FLASH_SER_IN PINB0
#define FLASH_SHLD PORTB2
#define FLASH_CLK PORTB1

//DDR has to be set manually in main()
#define FADER PORTC
#define FADER_IN PINC
/*
#define FADER_A PORTC1
#define FADER_B PORTC2
#define FADER_C PORTC3
#define FADER_D PORTC4
*/
#define FADER_CNT PORTC1
#define FADER_CLR PORTC2
#define FADER_CI PINC0
#define FADER_DELTA 50

#define TX PORTD1
#define P_SCL PORTC5
#define P_SDA PORTC4

#define TOV 0


int main(void)
{
	//_delay_ms(1000);
	cli();
	
	DDRD |= 0b11011110; //Configure DDRD Input Output (Register for KEYS)
	DDRB |= 0b00111110; //Configure DDRB Input Output (Register for FLASH)
	DDRC |= 0b00011110; //Configure DDRC Input Output (Register for FADER)
	
	UBRR0H = ((F_CPU / (16L * DMX_BAUD)) - 1) >> 8; // set baud rate
	UBRR0L = ((F_CPU / (16L * DMX_BAUD)) - 1);
	UCSR0B = (1<<TXEN0); //Transmit enable
	UCSR0C = (3 << UCSZ00) | (1 << USBS0); //Frame format: 8-bit, 2 Stop Bit
	
	//clear KEYS Output Shift register
	keys_write();
	for(int i = 0; i < 16; i++) keys_shift();
	
	//setup KEYS Input Shift register
	KEYS &= ~(1 << KEYS_SHLD);
	
	//setup FLASH Input Shift register
	FLASH &= ~(1 << FLASH_SHLD);
	
	//Setup ADC
	ADMUX |= (1 << ADLAR) | (1 << REFS0);
	ADCSRA |= (1 << ADEN) | (0b110 << ADPS0);
	
	
	//Clear Fader Counter
	FADER &= ~(1 << FADER_CLR);
	FADER |= (1 << FADER_CLR);
	
	//Init fader array
	for(int j = 0; j < FADER_MEASUREMENTS; j++)
	{
		fader_read(1);
	}
	
	//TWI Setup
	TWSR = 0;
	TWBR = ((F_CPU/F_SCL)-16)/2;	//Configure BaudRateRegister for 100kHz (Prescaler = 0)
	PORTC |= (1 << P_SCL) | (1 << P_SDA);
	
	//DMX Timer
	TCCR0B |= (1 << CS02);
	
	wdt_enable(WDTO_500MS);
	
	
    while(1)
    {
		wdt_reset();
		keys_read();
		flash_read();
		fader_read(0);
		if((TIFR0 & (1 << TOV)))
		{
			if(dmx_timer_ovf > DMX_TIMER_OVF_COUNT)
			{
				dmx_send();
				dmx_timer_ovf = 0;
				fader_read(1);
			}
			else dmx_timer_ovf++;
			TIFR0 |= (1 << TOV);
		}
		_delay_us(50);
		//flash_send(1, 3, 1);
		//print_keys();
		//print_flash();
		//print_fader();
		//_delay_ms(1000);
    }
	return 0;
}


//Debug

void print_keys()
{
	for(int i = 0; i < 64; i++)
	{
		if(keys[i/8] & (1 << (i%8))) write_buffer('1'); 
		else write_buffer('0');
		
		if(i%8 == 7) write_buffer('\n');
	}
	write_buffer('\n');
}

void print_flash()
{
	for(int i = 0; i < 24; i++)
	{
		if(flashs[i/8] & (1 << (i%8))) write_buffer('1');
		else write_buffer('0');
		
		if(i%8 == 7) write_buffer('\n');
	}
	write_buffer('\n');
}

void print_fader()
{
	for(int i = 0; i < FADER_COUNT; i++)
	{
		write_buffer(faders[i]);
		write_buffer(' ');
	}
	write_buffer('\n');
}


//Communication

void write_buffer(char c)
{
	tx_buffer[tx_write_pos] = c;
	tx_write_pos++;
	

	if (tx_write_pos >= TX_BUFFERSIZE)
	tx_write_pos = 0;
	
	if (UCSR0A & (1 << UDRE0)) //Reset transmission when transmit buffer is empty
	{
		UDR0 = 0;
	}
}

/*ISR(USART_TX_vect)
{
	if (tx_read_pos != tx_write_pos)
	{
		UDR0 = tx_buffer[tx_read_pos];
		tx_read_pos++;

		if (tx_read_pos >= TX_BUFFERSIZE)
		tx_read_pos = 0;
	}
}
*/

//KEYS Functions

void keys_write()
{
	KEYS |= (1 << KEYS_SER_OUT);
	KEYS |= (1 << KEYS_SCLK);
	_delay_loop_1(C_DELAY);
	KEYS &= ~(1 << KEYS_SCLK);
	_delay_loop_1(C_DELAY);
	KEYS |= (1 << KEYS_RCLK);
	_delay_loop_1(C_DELAY);
	KEYS &= ~(1 << KEYS_RCLK);
	_delay_loop_1(C_DELAY);
	KEYS &= ~(1 << KEYS_SER_OUT);
}

void keys_shift()
{
	KEYS |= (1 << KEYS_SCLK);
	_delay_loop_1(C_DELAY);
	KEYS &= ~(1 << KEYS_SCLK);
	_delay_loop_1(C_DELAY);
	KEYS |= (1 << KEYS_RCLK);
	_delay_loop_1(C_DELAY);
	KEYS &= ~(1 << KEYS_RCLK);
	_delay_loop_1(C_DELAY);
}

void keys_read_line()
{
	keys_load();
	
	uint8_t ser_in = 0;
	uint8_t change = 0;
	uint8_t pos = 0;
	for(uint8_t i = 0; i < 8; i++)
	{
		pos = (1 << (i));
		ser_in = (KEYS_IN & (1 << KEYS_SER_IN)) >> KEYS_SER_IN;
		change = (keys[keys_read_pos/8] & pos) ^ (ser_in << ((i)));
		if(change)
		{
			if(ser_in)
			{
				if(keys_send(keys_read_pos/8, (i), 1)) keys[keys_read_pos/8] |= pos;
				else break;
			}
			else 
			{
				if(keys_send(keys_read_pos/8, (i), 0)) keys[keys_read_pos/8] &= ~(pos);
				else break;
			}
		}
		keys_read_shift();
		keys_read_pos++;
		if(keys_read_pos >= 64) keys_read_pos = 0;
	}
}

void keys_read_shift()
{
	KEYS |= (1 << KEYS_CLK);
	_delay_loop_1(C_DELAY);
	KEYS &= ~(1 << KEYS_CLK);
	_delay_loop_1(C_DELAY);
}

void keys_read()
{
	memset(i2c_buffer, 0, sizeof(i2c_buffer));
	i2c_buffer_pos = 0;
	keys_write();
	for(uint8_t i = 0; i < 8; i++)
	{
		keys_read_line();
		keys_shift();
	}
	i2c_write_buffer(A_HDI);
}

void keys_load()
{
	KEYS &= ~(1 << KEYS_SHLD);
	_delay_loop_1(C_DELAY);
	KEYS |= (1 << KEYS_SHLD);
	_delay_loop_1(C_DELAY);
}

uint8_t keys_send(uint8_t l, uint8_t c, uint8_t s)
{
	char ch = 0;
	ch += (l << 5);
	ch += (c << 2);
	ch += (s << 1);
	
	if(i2c_buffer_pos >=I2C_BUFFERSIZE) return 0;
	i2c_buffer[i2c_buffer_pos] = ch;
	i2c_buffer_pos++;
	return 1;
}


//FLASH Functions

void flash_shift()
{
	switch (flash_state)
	{
		case 0:
		{
			FLASH &= ~(1 << FLASH_OUT_2);
			FLASH |= (1 << FLASH_OUT_0);
			flash_state++;
			break;
		}
		case 1:
		{
			FLASH &= ~(1 << FLASH_OUT_0);
			FLASH |= (1 << FLASH_OUT_1);
			flash_state++;
			break;
		}
		case 2:
		{
			FLASH &= ~(1 << FLASH_OUT_1);
			FLASH |= (1 << FLASH_OUT_2);
			flash_state = 0;
			break;
		}
		default:
		{
			flash_state = 0;
			break;
		}
	}
	_delay_loop_1(C_DELAY);
}

void flash_read_line()
{
		flash_load();
		
		uint8_t ser_in = 0;
		uint8_t change = 0;
		uint8_t pos = 0;
		for(uint8_t i = 0; i < 8; i++)
		{
			pos = (1 << (7-i));
			ser_in = (FLASH_IN & (1 << FLASH_SER_IN)) >> FLASH_SER_IN;
			change = (flashs[flash_read_pos/8] & pos) ^ (ser_in << ((7-i)));
			if(change)
			{
				if(ser_in)
				{
					if(flash_send(flash_read_pos/8, (7-i), 1)) flashs[flash_read_pos/8] |= pos;
					else break;
				}
				else
				{
					if(flash_send(flash_read_pos/8, (7-i), 0)) flashs[flash_read_pos/8] &= ~(pos);
					else break;
				}
			}
			flash_read_shift();
			flash_read_pos++;
			if(flash_read_pos >= 24) flash_read_pos = 0;
		}
}

void flash_read_shift()
{
	FLASH |= (1 << FLASH_CLK);
	_delay_loop_1(C_DELAY);
	FLASH &= ~(1 << FLASH_CLK);
	_delay_loop_1(C_DELAY);
}

void flash_load()
{
	FLASH &= ~(1 << FLASH_SHLD);
	_delay_loop_1(C_DELAY);
	FLASH |= (1 << FLASH_SHLD);
	_delay_loop_1(C_DELAY);
}

void flash_read()
{
	memset(i2c_buffer, 0, sizeof(i2c_buffer));
	i2c_buffer_pos = 0;
	for(uint8_t i = 0; i < 3; i++)
	{
		flash_shift();
		flash_read_line();
	}
	i2c_write_buffer(A_MIDI);
}

uint8_t flash_send(uint8_t l, uint8_t c, uint8_t s)
{
	char ch = 0;
	ch += (l << 5);
	ch += (c << 2);
	ch += (s << 1);
	
	
	if(i2c_buffer_pos >= I2C_BUFFERSIZE) return 0;
	i2c_buffer[i2c_buffer_pos] = ch;
	i2c_buffer_pos++;
	return 1;
}


//FADER

void fader_shift()
{
	fader_state++;
	if(fader_state >= FADER_COUNT) 
	{
		fader_state = 0;
		FADER &= ~(1 << FADER_CLR);
		FADER |= (1 << FADER_CLR);
	}
	else
	{
		FADER |= (1 << FADER_CNT);
		FADER &= ~(1 << FADER_CNT);
	}
	/*
	if(fader_state & 1) FADER |= (1 << FADER_A);
	else FADER &= ~(1 << FADER_A);
	if(fader_state & 2) FADER |= (1 << FADER_B);
	else FADER &= ~(1 << FADER_B);
	if(fader_state & 4) FADER |= (1 << FADER_C);
	else FADER &= ~(1 << FADER_C);
	if(fader_state & 8) FADER |= (1 << FADER_D);
	else FADER &= ~(1 << FADER_D);
	*/
}

void fader_read_value(uint8_t pos, uint8_t init)
{
	ADCSRA |= (1 << ADSC);
	while(ADCSRA & (1 << ADSC))
	{
	}
	uint8_t pre_pos = fader_write_pos-1;
	if(fader_write_pos == 0) pre_pos = FADER_MEASUREMENTS-1;
	uint8_t pre = fader_raw[pos][pre_pos];
	if(!init)
	{
		if((ADCH > pre && (ADCH - pre) > FADER_DELTA) || (ADCH < pre && (pre - ADCH) > FADER_DELTA)) fader_raw[pos][fader_write_pos] = pre;
		else fader_raw[pos][fader_write_pos] = ADCH;
	}
	else
	{
		fader_raw[pos][fader_write_pos] = ADCH;
	}
}

void fader_average()
{
	uint16_t ave;
	for(uint8_t i = 0; i < FADER_COUNT; i++)
	{
		ave = 0;
		for(uint8_t j = 0; j < FADER_MEASUREMENTS; j++)
		{
			ave += fader_raw[i][j];
		}
		ave /= FADER_MEASUREMENTS;
		if(ave < 3) ave = 0;
		if(ave > 252) ave = 255;
		if(faders[i] != ave)
		{
			faders[i] = ave;
			//fader_send(i, ave);
		}
	}
}

void fader_read(uint8_t init)
{
	for(int i = 0; i < FADER_COUNT; i++)
	{
		fader_shift();
		_delay_loop_1(1);
		fader_read_value(i, init);
	}	
	fader_average();
	fader_write_pos++;
	if(fader_write_pos >= FADER_MEASUREMENTS) fader_write_pos = 0;
}

/*void fader_send(uint8_t chan, uint8_t val)
{
	write_buffer(chan);
	write_buffer(val);
}
*/

//DMX

void dmx_send()
{
	//Start Byte
	UCSR0B &= ~(1 << TXEN0);
	PORTD &= ~(1 << TX);
	_delay_us(130); //88us warten, 130 fuer schlechtes Equipment
	PORTD |= (1 << TX);
	_delay_us(4); //4us warten
	UCSR0B |= (1 << TXEN0);
	UDR0 = 0; //Start Byte
	while ( !( UCSR0A & (1<<UDRE0)));
	
	//Channels
	for(uint8_t i = 0; i < FADER_COUNT; i++)
	{
		_delay_us(10);
		UDR0 = faders[i];
		while ( !( UCSR0A & (1<<UDRE0)));
	}
	for(uint16_t i = FADER_COUNT; i < 512; i++)
	{
		_delay_us(10);
		UDR0 = 0;
		while(!(UCSR0A & (1 << UDRE0)));
	}
}




//TWI

/*************************************************************************	
  Issues a start condition and sends address and transfer direction.
  return 0 = device accessible, 1= failed to access device
*************************************************************************/
unsigned char i2c_start(unsigned char address)
{
    uint8_t   twst;

	// send START condition
	TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);

	// wait until transmission completed
	while(!(TWCR & (1<<TWINT)));

	// check value of TWI Status Register. Mask prescaler bits.
	twst = TW_STATUS & 0xF8;
	if ( (twst != TW_START) && (twst != TW_REP_START)) return 1;

	// send device address
	TWDR = address;
	TWCR = (1<<TWINT) | (1<<TWEN);

	// wail until transmission completed and ACK/NACK has been received
	while(!(TWCR & (1<<TWINT)));

	// check value of TWI Status Register. Mask prescaler bits.
	twst = TW_STATUS & 0xF8;
	if ( (twst != TW_MT_SLA_ACK) && (twst != TW_MR_SLA_ACK) ) return 1;

	return 0;

}/* i2c_start */


/*************************************************************************
 Issues a start condition and sends address and transfer direction.
 If device is busy, use ack polling to wait until device is ready
 
 Input:   address and transfer direction of I2C device
*************************************************************************/
void i2c_start_wait(unsigned char address)
{
    uint8_t   twst;


    while ( 1 )
    {
	    // send START condition
	    TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
    
    	// wait until transmission completed
    	while(!(TWCR & (1<<TWINT)));
    
    	// check value of TWI Status Register. Mask prescaler bits.
    	twst = TW_STATUS & 0xF8;
    	if ( (twst != TW_START) && (twst != TW_REP_START)) continue;
    
    	// send device address
    	TWDR = address;
    	TWCR = (1<<TWINT) | (1<<TWEN);
    
    	// wail until transmission completed
    	while(!(TWCR & (1<<TWINT)));
    
    	// check value of TWI Status Register. Mask prescaler bits.
    	twst = TW_STATUS & 0xF8;
    	if ( (twst == TW_MT_SLA_NACK )||(twst ==TW_MR_DATA_NACK) ) 
    	{    	    
    	    /* device busy, send stop condition to terminate write operation */
	        TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
	        
	        // wait until stop condition is executed and bus released
	        while(TWCR & (1<<TWSTO));
	        
    	    continue;
    	}
    	//if( twst != TW_MT_SLA_ACK) return 1;
    	break;
     }

}/* i2c_start_wait */


/*************************************************************************
 Issues a repeated start condition and sends address and transfer direction 
 Input:   address and transfer direction of I2C device
 
 Return:  0 device accessible
          1 failed to access device
*************************************************************************/
unsigned char i2c_rep_start(unsigned char address)
{
    return i2c_start( address );

}/* i2c_rep_start */


/*************************************************************************
 Terminates the data transfer and releases the I2C bus
*************************************************************************/
void i2c_stop(void)
{
    /* send stop condition */
	TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
	
	// wait until stop condition is executed and bus released
	while(TWCR & (1<<TWSTO));

}/* i2c_stop */


/*************************************************************************
  Send one byte to I2C device
  
  Input:    byte to be transfered
  Return:   0 write successful 
            1 write failed
*************************************************************************/
unsigned char i2c_write( unsigned char data )
{	
    uint8_t   twst;
    
	// send data to the previously addressed device
	TWDR = data;
	TWCR = (1<<TWINT) | (1<<TWEN);

	// wait until transmission completed
	while(!(TWCR & (1<<TWINT)));

	// check value of TWI Status Register. Mask prescaler bits
	twst = TW_STATUS & 0xF8;
	if( twst != TW_MT_DATA_ACK) return 1;
	return 0;

}/* i2c_write */


unsigned char i2c_write_buffer(char address)
{
	if(i2c_buffer_pos == 0) return 0;
	if(i2c_start(address)) return 1;
	for(uint8_t i = 0; i < i2c_buffer_pos; i++) if(i2c_write(i2c_buffer[i]))
	{
		uint8_t j = 0;
		while(j<3 && i2c_write(i2c_buffer[i]));
	}
	i2c_stop();
	i2c_buffer_pos = 0;
	return 0;
}