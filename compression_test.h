#ifndef compression_test
#define compression_test


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

int compress_file(FILE* file, char* filename, int bitsize); /* returns 0 or -1 for success, store somewhere? */

int decompress_file(FILE* file, char* filename); /* returns 0 or -1 for success, store somewhere? */

#endif