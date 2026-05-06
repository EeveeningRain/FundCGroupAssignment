/* Include header file */
#include "compress.h"

/****************************************************************************************************
 * Conceptualisation of LZW Encoding from wikipedia:
 * 
 * LZW ENCODING
 *   *     PSEUDOCODE
 *   1     Initialize table with single character strings
 *   2     P = first input character
 *   3     WHILE not end of input stream
 *   4          C = next input character
 *   5          IF P + C is in the string table
 *   6            P = P + C
 *   7          ELSE
 *   8            output the code for P
 *   9          add P + C to the string table
 *   10           P = C
 *   11         END WHILE
 *   12    output code for P 
 * 
 * note concept of the LZ triple, noted that making a struct that can store a piece of compressed data 
 * may help.
 * 
 * after that point, convert text in to lz triples
 * 
 * pattern match (any algorithm..)
 * 
 * convert triples to bits
 * 
*****************************************************************************************************/

/****************************************************************************************************
 * Main algorithms for lz77 compression / decompression
 * 
 * lz77_compress takes in a text (uint8_t*) and it's size, and then outputs a string which represents
 * the compressed text. Ptr length width controls how large a match can be.
 * 
 * lz77_decompress takes in a compressed lz77 text and decompresses it by looping through progressively
 * each match and replacing them with the signature which represents the pattern
 * 
 ****************************************************************************************************/

/******************************************************************************************
 * Main lempel-ziv 77 compression algorithm function, takes in an uncompressed text via a char*
 * and then compresses it by replacing seen sequences with byte data representing instructions
 * to copy that sequence. Compression works as the space required for a match signature is less
 * than the data itself, but only works well on binary data or large text data due to the need
 * for lots of repeating data.
 * 
 * inputs:
 * - uint8* (char) uncompressed_text, uint32_t uncompressed_size (error handling),
 *   uint8* (char) compressed_text, uint8_t pointer_length_width (size of matches)
 * outputs:
 * - compressed_text is filled with data, uint32_t to represent compressed size
*******************************************************************************************/
uint32_t lz77_compress (const uint8_t *uncompressed_text, const uint32_t uncompressed_size, uint8_t *compressed_text, const uint8_t token_length_width)
{
    /* variable declarations for algorithm */
    uint16_t token_ptr, token_copy_pos, temp_token_copy_pos, token_length, temp_token_length;
    uint32_t data_ptr, output_size, buffer_pos, token_lookahead_ref, look_behind, look_ahead;

    /* consts for the max parameters of a token (based on token_length_width param) */
    const uint16_t TOKEN_COPY_POS_MAX = pow(2, 16 - token_length_width);
    const uint16_t TOKEN_LENGTH_MAX = pow(2, token_length_width);

    /* Storing the uncompressed size in the first 4 bytes of the compressed text */
    *((uint32_t *) compressed_text) = uncompressed_size;

    /*Storing the token_length_width at the 5th byte of the compressed text */
    *(compressed_text + 4) = token_length_width;

    /* Start writing compressed data at the 6th byte*/
    data_ptr = output_size = 5;
    
    /* Looping through the entire uncompressed text until our buffer pos reaches the end */
    for(buffer_pos = 0; buffer_pos < uncompressed_size; ++buffer_pos)
    {
        token_copy_pos = 0;
        token_length = 0;
        
        for(temp_token_copy_pos = 1; (temp_token_copy_pos < TOKEN_COPY_POS_MAX) && (temp_token_copy_pos <= buffer_pos); ++temp_token_copy_pos)
        {
            look_behind = buffer_pos - temp_token_copy_pos;
            look_ahead = buffer_pos;
            for(temp_token_length = 0; uncompressed_text[look_ahead++] == uncompressed_text[look_behind++]; ++temp_token_length)
                if(temp_token_length == TOKEN_LENGTH_MAX)
                    break;
            if(temp_token_length > token_length)
            {
                token_copy_pos = temp_token_copy_pos;
                token_length = temp_token_length;
                if(token_length == TOKEN_LENGTH_MAX)
                    break;
            }
        }
        buffer_pos += token_length;
        if((buffer_pos == uncompressed_size) && token_length)
        {
            token_ptr = (token_length == 1) ? 0 : ((token_copy_pos << token_length_width) | (token_length - 2));
            token_lookahead_ref = buffer_pos - 1;
        }
        else
        {
            token_ptr = (token_copy_pos << token_length_width) | (token_length ? (token_length - 1) : 0);
            token_lookahead_ref = buffer_pos;
        }
        *((uint16_t *) (compressed_text + data_ptr)) = token_ptr;
        data_ptr += 2;
        *(compressed_text + data_ptr++) = *(uncompressed_text + token_lookahead_ref);
        output_size += 3;
    }

    return output_size;
}

/******************************************************************************************
 * Main Lempel-Ziv 77 decompression algorithm, simpler than compression, reads in compressed
 * text, gets the uncompressed size stored in the first 4 bytes, then loops expanding each
 * match signature to the full uncompressed state by reading back to the stored match.
 * 
 * inputs:
 * - File ptr *in
 * outputs:
 * - Size of file as a long
*******************************************************************************************/
uint32_t lz77_decompress (const uint8_t *compressed_text, uint8_t *uncompressed_text)
{
    /* variable declarations for algorithm */
    uint8_t token_length_width; /*token length width, how much to copy in to a token*/

    /* token_ptr stores individual tokens, token length / pos specifies how far back
       and how much to copy for a match, mask is used to extract length bits */
    uint16_t token_ptr, token_length, token_copy_pos, token_length_mask;

    /*buffer_pos stores position in our "stream" of compressed text, data_ptr is
      how far we are in through the data in bytes, copy offset used to read back */
    uint32_t buffer_pos, data_ptr, copy_offset, uncompressed_size;

    /* uncompressed size is stored in first 4 bytes of compressed text, grab data */
    uncompressed_size = (uint32_t)compressed_text[0] |
        ((uint32_t)compressed_text[1] << 8) |
        ((uint32_t)compressed_text[2] << 16) |
        ((uint32_t)compressed_text[3] << 24);

    /* fifth byte is the token length - how large matches are in the compression*/
    token_length_width = *(compressed_text + 4);

    /* start reading actual data from the 5th index (6th byte) */
    data_ptr = 5;

    /* used to extract length from a token; bit math */
    token_length_mask = (1U << token_length_width) - 1;

    /* we keep looping as long as our position is less than the final, uncompressed size */
    buffer_pos = 0;
    while(buffer_pos < uncompressed_size)
    {
        /* read in a token at data_ptr and move cursor forward */
        token_ptr = *((uint16_t *)(compressed_text + data_ptr));
        data_ptr += 2;

        /* use mask and length to read in the two fields in a token */
        token_copy_pos = token_ptr >> token_length_width; /* where we need to copy from */
        token_length = token_copy_pos ? ((token_ptr & token_length_mask) + 1) : 0; /* how much to copy */

        if (token_copy_pos > buffer_pos) {
            printf("Invalid backreference: %u > %u\n", token_copy_pos, buffer_pos);
            return 0;
        }

        /* this is where we now replace the token with data */
        if(token_copy_pos) { 
            copy_offset = buffer_pos - token_copy_pos;
            /* offset is current pos minus token's copy pos; loop until gone through length specified*/
            uint32_t i = 0;
           for(; i < token_length; i++){
                /* add to uncompressed text each byte as we go through */
                /*if (buffer_pos > uncompressed_size){
                    printf("Buffer overflowed text! buffer pos: %d, uncompressed size: %d\n", buffer_pos, uncompressed_size);
                    return 0;
                }
                if (token_copy_pos > buffer_pos) {
                    printf("Invalid offset: %u > %u\n", token_copy_pos, buffer_pos);
                    return 0;
                }*/
                uncompressed_text[buffer_pos++] = uncompressed_text[copy_offset++];
            }
        }
        uncompressed_text[buffer_pos++] = *(compressed_text + data_ptr++);
    }
    
    /* edge case - drift in buffer pos, couldn't figure out why */
    if(buffer_pos - uncompressed_size == 2) {
        uncompressed_text[uncompressed_size] = EOF;
        buffer_pos = uncompressed_size;
    }
    return buffer_pos; /* should be positioned at EOF - is the same as length */
}

/****************************************************************************************************
 * Functions relating to the compression / decompression of specific files:
 * fsize is a helper function that simply gets the length of a file using ftell/fseek
 * 
 * file_lz77_compress takes in a file, outputs a file, and specifies how much memory to use and the 
 * pointer length width
 * > is used in a loop to iteratively compress the file based on ptr length
 * 
 * file_lz77_decompress takes in a file (compressed), outputs a file which is decompressed
 * 
 ****************************************************************************************************/

/******************************************************************************************
 * This helper function takes in a file, and gets the size of the file by seeking
 * the file pointer to the end, getting the length, and then setting the pointer
 * back to the start of the file.
 * 
 * inputs:
 * - File ptr *in
 * outputs:
 * - Size of file as a long
*******************************************************************************************/
long fsize (FILE *in)
{
    long pos, length;
    pos = ftell(in);
    fseek(in, 0L, SEEK_END);
    length = ftell(in);
    fseek(in, pos, SEEK_SET);
    return length;
}

/* CONSIDER NEW HELPER TO HELP OPTIMISE POINTER LENGTH!! (as a good addition) */

uint32_t lz77_compress_until_optimised(uint8_t* uncompressed_text, uint32_t uncompressed_size, uint8_t* compressed_text, const size_t malloc_size){
    /* magic numbers - found via testing */
    const uint32_t MAX_BITSIZE = 16;
    const uint32_t MIN_BITSIZE = 2;

    uint8_t* prev_text = malloc(malloc_size);
    uint32_t prev_size = UINT32_MAX;
    
    uint32_t cur_size = 0;

    /* iterate over bit sizes until we find the next bitsize increases size instead of decrease - return previous one instead */
    int i = MIN_BITSIZE;
    for(; i < MAX_BITSIZE; ++i){
        printf("Compressing to bitsize: %d\n", i);
        cur_size = lz77_compress(uncompressed_text,uncompressed_size, compressed_text, i);

        /* if new compression is better, set prev to new (kind of a best match) */
        if(cur_size < prev_size){
            memcpy(prev_text, compressed_text, cur_size);
            prev_size = cur_size;
        }
        /* if old compression better, we know it is the best, return old compression */
        else{
            memcpy(compressed_text, prev_text, prev_size);
            free(prev_text);
            return prev_size;
        }
    }

    /* if it never gets bigger, simply return compressed text */
    free(prev_text);
    return cur_size;
}

/******************************************************************************************
 * This function takes in data about a file, a maximum malloc size, and pointer length, 
 * and uses this information to write a new compressed file out of an uncompressed one
 * using the lempel-ziv compression algorithm. This method is meant to be used with progressively
 * larger pointer lengths to optimise the compression size.
 * 
 * inputs:
 * - char* filename_in, char* filename_out (file IO), size_t malloc_size (max size of new file),
 *   uint8_t pointer_length_width used for algorithm (how far to check for match)
 * outputs:
 * - compressed file, uint32_t size of compressed file
*******************************************************************************************/
uint32_t file_lz77_compress (const char *filename_in, const char *filename_out, const size_t malloc_size)
{
    /* vars for use in method, *in and *out are the files, we have pointers for the texts themselves, and then sizes*/
    FILE *in, *out;
    uint8_t *uncompressed_text, *compressed_text;
    uint32_t uncompressed_size, compressed_size;


    /* try to open the file, if it fails, immediately return 0 */
    in = fopen(filename_in, "rb");
    if(in == NULL)
        return 0;
    
    /*use this file to verify uncompressed size, malloc for the uncompressed text that is going to be read in */
    uncompressed_size = fsize(in);
    uncompressed_text = malloc(uncompressed_size);

    /* try to read in entire file, if fails then return 0*/
    if((uncompressed_size != fread(uncompressed_text, 1, uncompressed_size, in)))
        return 0;

    /* close file */
    fclose(in);

    /* this is where we actually compress the text, malloc_size is a large buffer, then we pass in data to func */
    compressed_text = malloc(malloc_size);

    compressed_size = lz77_compress_until_optimised(uncompressed_text, uncompressed_size, compressed_text, malloc_size);

    /* once compressed, write out to file, if opening or writing fails return 0 */
    out = fopen(filename_out, "wb");
    if(out == NULL)
        return 0;
    if((compressed_size != fwrite(compressed_text, 1, compressed_size, out)))
        return 0;
    fclose(out);

    /* on success return the new size of the compressed file */
    return compressed_size;
}

/******************************************************************************************
 * This function takes in a compressed file, and uses it to decompress it in to a
 * new file.
 * 
 * inputs:
 * - char* filename_in (compressed), char* filename_out (uncompressed)
 * outputs:
 * - uncompressed file, uint32_t size of uncompressed file
*******************************************************************************************/
uint32_t file_lz77_decompress (const char *filename_in, const char *filename_out)
{
    /* vars for use in method, *in and *out are the files themselves, then the texts and sizes*/
    FILE *in, *out;
    uint32_t compressed_size, uncompressed_size;
    uint8_t *compressed_text, *uncompressed_text;

    /* try to open the compressed file, if fail return 0 */
    in = fopen(filename_in, "rb");
    if(in == NULL)
        return 0;

    /* grab size of this file, malloc enough space to read in */
    compressed_size = fsize(in);
    compressed_text = malloc(compressed_size);

    /* if read fails, return 0 */
    if(fread(compressed_text, 1, compressed_size, in) != compressed_size)
        return 0;
    fclose(in);

    /* compression has stored the compressed size in the first 4 bytes of the compressed text, read these in */
    uncompressed_size = *((uint32_t *) compressed_text);
    /* and malloc */
    uncompressed_text = malloc(uncompressed_size);

    /* try to decompress, if it fails then return 0 */
    if(lz77_decompress(compressed_text, uncompressed_text) != uncompressed_size){
        printf("Decompression failed! Uncompressed size (%d) did not equal returned size!\n", uncompressed_size);
        return 0;
    }
    /*printf("Compression seemed to succeed!\n");*/
    /* then write to file checking for failing to open or write*/
    out = fopen(filename_out, "wb");
    if(out == NULL){
        printf("Filename invalid! filename_out: %s\n", filename_out);
        return 0;
    }
    if(fwrite(uncompressed_text, 1, uncompressed_size, out) != uncompressed_size){
        printf("Written text unequal to uncompressed size!\n");
        return 0;
    }
    fclose(out);


    /* return new, uncompressed size */
    return uncompressed_size;
}