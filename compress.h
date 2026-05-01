#ifndef compression
#define compression
/* lz77 compression */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

uint32_t encode(const uint8_t *i, uint32_t isz, uint8_t *o, uint8_t w);
uint32_t decode(const uint8_t *i, uint8_t *o);


#endif