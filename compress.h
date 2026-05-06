/*******************************************************************************
 * Header file for Lempel-Ziv 77 Compression / Decompression implementation
 */

#ifndef compression
#define compression

/*******************************************************************************
 * Header files & Method lists
*******************************************************************************/
#include <stdio.h> 
    /* fopen(), fsize(), ftell(), fseek(), fread(), fclose(). fwrite() */
#include <stdlib.h> 
    /* FILE, malloc(), */
#include <math.h> 
    /* pow() */

/*stdint.h is used to simplify types for compression - could use primitives instead */
#include <stdint.h> 
    /* uint8, uint16, uint32 */


/*******************************************************************************
 * Function prototypes
*******************************************************************************/
/*Compression / decompression algorithms*/
uint32_t lz77_compress (const uint8_t *uncompressed_text, const uint32_t uncompressed_size, uint8_t *compressed_text, const uint8_t pointer_length_width);
uint32_t lz77_decompress (const uint8_t *compressed_text, uint8_t *uncompressed_text);

/* File handling functions */
long fsize (FILE *in); /* Helper function */
uint32_t lz77_compress_until_optimised(uint8_t* uncompressed_text, uint32_t uncompressed_size, uint8_t* compressed_text, const size_t malloc_size); /* Helper function */
uint32_t file_lz77_compress (const char *filename_in, const char *filename_out, const size_t malloc_size);
uint32_t file_lz77_decompress (const char *filename_in, const char *filename_out);


#endif