/*******************************************************************************
 * Header file for Lempel-Ziv 77 Compression / Decompression implementation
 */

#ifndef compression
#define compression

#define MALLOC_SIZE 10000000
/*******************************************************************************
 * Header files & Method lists
*******************************************************************************/
#include <stdio.h> 
    /* fopen(), fsize(), ftell(), fseek(), fread(), fclose(), fwrite(), ssize_t */
#include <stdlib.h> 
    /* FILE, malloc(), */
#include <math.h> 
    /* pow() */
#include <string.h>
    /* memcpy() */


/*******************************************************************************
 * Function prototypes
*******************************************************************************/
/*Compression / decompression algorithms*/
unsigned int lz77_compress (
    const unsigned char *uncompressed_text, 
    const unsigned int uncompressed_size, 
    unsigned char *compressed_text, 
    const unsigned char token_length_width);
unsigned int lz77_decompress (
    const unsigned char *compressed_text, 
    unsigned char *uncompressed_text);

/* File handling functions */
unsigned long fsize (FILE *in); /* Helper function */
unsigned int lz77_compress_until_optimised(
    unsigned char* uncompressed_text, 
    unsigned int uncompressed_size, 
    unsigned char* compressed_text, 
    const size_t malloc_size); /* Helper function */
unsigned int file_lz77_compress (
    const char *filename_in, 
    const char *filename_out, 
    const size_t malloc_size);
unsigned int file_lz77_decompress (
    const char *filename_in, 
    const char *filename_out);
unsigned int do_compression(
    const unsigned int mode, 
    const char *infile, 
    const char *outfile);


#endif