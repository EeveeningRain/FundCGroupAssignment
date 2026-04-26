#ifndef compression
#define compression
/* lz77 compression */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int encode(const char *i, int isz, char *o, char w);
int decode(const char *i, char *o);


#endif