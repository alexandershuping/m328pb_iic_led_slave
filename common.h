#include <avr/io.h>

#pragma once

typedef uint8_t bool;
#define true 1
#define false 0

uint16_t map(uint16_t x, uint32_t inmin, uint32_t inmax, uint32_t outmin, uint32_t outmax){
	return (uint16_t)(((uint32_t)x - inmin) * (outmax - outmin) / (inmax - inmin) + outmin);
}
