#ifndef compression
#define compression
/* lz77 compression */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* use of char for an 8-bit integer */
int lz77_compress_from_file(char *filename_in, char *filename_out, size_t malloc_size, char pointer_length_width);
int lz77_compress(char *uncompressed_text, int uncompressed_size, char *compressed_text, char pointer_length_width);


#endif