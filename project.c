/* 
   Copyright 2018 Alexander Shuping

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 
 * project.c
 * test project for the library
 */

#ifndef F_CPU
	#define F_CPU 8000000UL
#endif

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/twi.h>

#include <iic/iic.h>
#include <iic/iic_extras.h>
#include "common.h"

#define SLAVE

#define ADDRESS 0x31
#define REMOTE 0x20
#define BITRATE_PRESCALER 0
#define BITRATE 0
#include "gamma_lut.h"

typedef struct color_t{
	uint16_t r;
	uint16_t g;
	uint16_t b;
} color_t;

volatile uint16_t lutstep = 0;
volatile uint8_t colorstep = 0;
volatile bool change = false;

typedef enum{
	PATTERN_DEMO = 0,
	PATTERN_CONST = 1,
	PATTERN_SINE = 2
} pattern_t;

volatile color_t primary;
volatile pattern_t pattern = PATTERN_DEMO;

volatile IIC_COMMAND_t current_command = 0;
volatile uint8_t command_index = 0;
volatile uint8_t channel_select = 0; // 0 = RED; 1 = GRN; 2 = BLU

volatile uint8_t stored_data = 0;

uint8_t iic_callback_fun(volatile iic_t *iic, uint8_t received_data){
	if(iic -> error_state != IIC_NO_ERROR){
		PORTD = PORTD ^ (1 << PD6); // toggle an error LED
	}

	if(iic -> state == IIC_SLAVE_RECEIVER || current_command != 0){
		if(current_command == 0){
			current_command = received_data;
			command_index = 0;
		}else{
			switch(current_command){

				// Respond to LED color write command
				case IIC_COMMAND_LED_WRITE_WORD:
					switch(command_index++){
						case 0:
							channel_select = received_data;
							break;
						case 1:
							stored_data = received_data;
							break;
						case 2:
							current_command = 0;
							switch(channel_select){
								case 0:
									primary.r = stored_data | (received_data << 8);
									break;
								case 1:
									primary.g = stored_data | (received_data << 8);
									break;
								case 2:
									primary.b = stored_data | (received_data << 8);
									break;
							}
					}
					break;

				// Respond to LED pattern set command
				case IIC_COMMAND_LED_SET_PATTERN:
					current_command = 0;
					pattern = received_data;
					break;

				// Unsupported command
				default:
					current_command = 0;
					break;
			}
		}
	}

	if(iic -> state == IIC_SLAVE_TRANSMITTER){
		return stored_data;
	}

	return 0;
}

int main(void){
	// Setup things
	
	primary.r = 0x333;
	primary.g = 0xccc;
	primary.b = 0xfff;
	
	DDRB |= (1 << DDB1);
	DDRD |= (1 << DDD0) | (1 << DDD1);

	// Set up TC0 for wave-advance overflow
	TCCR0A = (1 << WGM00); // CTC mode
	TIMSK0 = (1 << OCIE0A);  // interrupt on overflow
	OCR0A = 2;
	TCCR0B = (1 << CS01) | (1 << CS00); // prescale divide by 1024

	// RED pwm on OC1A
	TCCR1A = (1 << COM1A1) | (1 << WGM11); // Clear on compare-match, set on bottom, use ICR1 for TOP.
	TCCR1B = (1 << WGM12) | (1 << WGM13) | (1 << CS10); // Fast PWM mode, no prescaler
	ICR1 = 0x0fff; // Use 12-bit PWM
	// GREEN pwm on OC3A
	TCCR3A = (1 << COM1A1) | (1 << WGM11); // Clear on compare-match, set on bottom, use ICR1 for TOP.
	TCCR3B = (1 << WGM12) | (1 << WGM13) | (1 << CS10); // Fast PWM mode, no prescaler
	ICR3 = 0x0fff; // Use 12-bit PWM
	// BLUE pwm on OC4A
	TCCR4A = (1 << COM1A1) | (1 << WGM11); // Clear on compare-match, set on bottom, use ICR1 for TOP.
	TCCR4B = (1 << WGM12) | (1 << WGM13) | (1 << CS10); // Fast PWM mode, no prescaler
	ICR4 = 0x0fff; // Use 12-bit PWM
	DDRD |= (1 << DDD6);
	
	setup_iic(ADDRESS, true, true, BITRATE, BITRATE_PRESCALER, 20, &iic_callback_fun);

	enable_iic();
	sei();

	while(1){
		if(IIC_MODULE.state == IIC_SLAVE_TRANSMITTER){
			PORTB |= (1 << PB3);
		}else{
			PORTB &= ~(1 << PB3);
		}

		/*uint16_t r = map(sinestep(brightness),0,4096,0,0x700);
		uint16_t g = map(sinestep(brightness),0,4096,0,0x100);
		uint16_t b = map(sinestep(brightness),0,4096,0,0xff0);*/
		if(change){
			change = false;
			uint16_t r;
			uint16_t g;
			uint16_t b;

			switch(pattern){
				case PATTERN_DEMO:
				case PATTERN_SINE:
					r = map(sinestep(lutstep), 0, 4095, 0, primary.r);
					g = map(sinestep(lutstep), 0, 4095, 0, primary.g);
					b = map(sinestep(lutstep), 0, 4095, 0, primary.b);
					break;

				case PATTERN_CONST:
				default:
					r = primary.r;
					g = primary.g;
					b = primary.b;
			}

			OCR1A = gammalut(r);
			OCR3A = gammalut(g);
			OCR4A = gammalut(b);
		}
	}

	return 0;
}

ISR(TIMER0_COMPA_vect){
	// overflows for each step in the LUT
	if(++lutstep > 1023){
		// we reached the end of our LUT
		lutstep = 0;
	}
	if(lutstep == 768 && pattern == PATTERN_DEMO){
		// change the color when the value is 0.
		switch(++colorstep){
			case 0:
				primary.r = 0x000;
				primary.g = 0xfff;
				primary.b = 0x999;
				break;
			case 1:
				primary.r = 0x666;
				primary.g = 0xfff;
				primary.b = 0x666;
				break;
			case 2:
				primary.r = 0xfff;
				primary.g = 0xfff;
				primary.b = 0x666;
				break;
			case 3:
				primary.r = 0xfff;
				primary.g = 0x666;
				primary.b = 0x000;
				break;
			case 4:
				primary.r = 0xfff;
				primary.g = 0x000;
				primary.b = 0x666;
				break;
			case 5:
				primary.r = 0xfff;
				primary.g = 0x666;
				primary.b = 0xfff;
				break;
			case 6:
				primary.r = 0x666;
				primary.g = 0x666;
				primary.b = 0xfff;
				break;
			case 7:
				primary.r = 0x000;
				primary.g = 0x666;
				primary.b = 0xfff;
				break;
			default:
				colorstep = 0;
				primary.r = 0x333;
				primary.g = 0xccc;
				primary.b = 0xfff;
		}
	}
	// tell the main loop to update with the new LUTSTEP (and possibly color) values
	change = true;
}
