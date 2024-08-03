/*
 * main.c
 *
 * Created: 8/10/2021 9:34:02 PM
 *  Author: Cedric
 */ 
#define F_CPU 16000000UL

#include <xc.h>
#include <avr/io.h>
#include <util/delay.h>
#include "tastenbeleuchtung.h"

#define KEYS_DELAY 100
#define FLASH_DELAY 100

#define KEYS_SIZE 8
#define FLASH_SIZE 3

#define KEYS_C 0
#define KEYS_R 1
#define FLASH_C 2

#define KEYS PORTD
#define KEYS_SRCLK_C PD0
#define KEYS_RCLK_C PD1
#define KEYS_SER_C PD2
#define KEYS_SRCLK_R PD3
#define KEYS_RCLK_R PD4
#define KEYS_SER_R PD5

#define FLASH PORTC
#define FLASH_SRCLK_C PC0
#define FLASH_RCLK_C PC1
#define FLASH_SER_C PC2
#define FLASH_R_0 PC3
#define FLASH_R_1 PC4
#define FLASH_R_2 PC5


int main(void)
{
	DDRD |= 0b00111111;
	DDRC |= 0b00111111;
    while(1)
    {
		keys_write();
		flash_write();
    }
}

void shift(uint8_t out)
{
	switch(out)
	{
		case KEYS_C:
		{
			KEYS |= (1 << KEYS_SRCLK_C);
			KEYS &= ~(1 << KEYS_SRCLK_C);
			break;
		}
		case KEYS_R:
		{
			KEYS |= (1 << KEYS_SRCLK_R);
			KEYS &= ~(1 << KEYS_SRCLK_R);
			break;
		}
		case FLASH_C:
		{
			FLASH |= (1 << FLASH_SRCLK_C);
			FLASH &= ~(1 << FLASH_SRCLK_C);
			break;
		}
	}
}

void latch(uint8_t out)
{
	switch(out)
	{
		case KEYS_C:
		{
			KEYS |= (1 << KEYS_RCLK_C);
			KEYS &= ~(1 << KEYS_RCLK_C);
			break;
		}
		case KEYS_R:
		{
			KEYS |= (1 << KEYS_RCLK_R);
			KEYS &= ~(1 << KEYS_RCLK_R);
			break;
		}
		case FLASH_C:
		{
			FLASH |= (1 << FLASH_RCLK_C);
			FLASH &= ~(1 << FLASH_RCLK_C);
			break;
		}
	}
}

void keys_write_line(uint8_t l)
{
	for(uint8_t i = 0; i < 8; i++)
	{
		if(keys[l] & (1 << i))
		{
			KEYS &= ~(1 << KEYS_SER_C);
		}
		else
		{
			KEYS |= (1 << KEYS_SER_C);
		}
		shift(KEYS_C);
	}
	latch(KEYS_C);
}

void keys_next_line()
{
	if(keys_current_line == 0)
	{
		KEYS |= (1 << KEYS_SER_R);
	}
	shift(KEYS_R);
	latch(KEYS_R);
	
	KEYS &= ~(1 << KEYS_SER_R);
	
	keys_current_line++;
	if(keys_current_line >= KEYS_SIZE) keys_current_line = 0;
}

void keys_write()
{
	for(uint8_t k = 0; k < KEYS_SIZE; k++)
	{
		keys_next_line();
		keys_write_line(k);
		_delay_us(KEYS_DELAY);
	}
}


//FLASH

void flash_write_line(uint8_t l)
{
	for(uint8_t i = 0; i < 8; i++)
	{
		if(flash[l] & (1 << i))
		{
			FLASH &= ~(1 << FLASH_SER_C);
		}
		else
		{
			FLASH |= (1 << FLASH_SER_C);
		}
		shift(FLASH_C);
	}
	latch(FLASH_C);
}

void flash_next_line()
{
	switch(flash_current_line)
	{
		case 0:
		{
			FLASH |= (1 << FLASH_R_0);
			FLASH &= ~(1 << FLASH_R_2);
			//flash_current_line++;
			break;
		}
		case 1:
		{
			FLASH |= (1 << FLASH_R_1);
			FLASH &= ~(1 << FLASH_R_0);
			//flash_current_line++;
			break;
		}
		case 2:
		{
			FLASH |= (1 << FLASH_R_2);
			FLASH &= ~(1 << FLASH_R_1);
			//flash_current_line = 0;
			break;
		}
		default:
		{
			flash_current_line = 0;
		}
	}
	
	flash_current_line++;
	if(flash_current_line >= FLASH_SIZE) flash_current_line = 0;
}

void flash_write()
{
	for(uint8_t i = 0; i < FLASH_SIZE; i++)
	{
		flash_next_line();
		flash_write_line(i);
		_delay_us(FLASH_DELAY);
	}
	FLASH &= ~(1 << FLASH_R_2);
}