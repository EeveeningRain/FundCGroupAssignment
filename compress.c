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

uint32_t lz77_compress (uint8_t *uncompressed_text, uint32_t uncompressed_size, uint8_t *compressed_text, uint8_t pointer_length_width)
{
    uint16_t pointer_pos, temp_pointer_pos, output_pointer, pointer_length, temp_pointer_length;
    uint32_t compressed_pointer, output_size, coding_pos, output_lookahead_ref, look_behind, look_ahead;
    uint16_t pointer_pos_max, pointer_length_max;
    pointer_pos_max = pow(2, 16 - pointer_length_width);
    pointer_length_max = pow(2, pointer_length_width);

    *((uint32_t *) compressed_text) = uncompressed_size;
    *(compressed_text + 4) = pointer_length_width;
    compressed_pointer = output_size = 5;
    
    for(coding_pos = 0; coding_pos < uncompressed_size; ++coding_pos)
    {
        pointer_pos = 0;
        pointer_length = 0;
        for(temp_pointer_pos = 1; (temp_pointer_pos < pointer_pos_max) && (temp_pointer_pos <= coding_pos); ++temp_pointer_pos)
        {
            look_behind = coding_pos - temp_pointer_pos;
            look_ahead = coding_pos;
            for(temp_pointer_length = 0; uncompressed_text[look_ahead++] == uncompressed_text[look_behind++]; ++temp_pointer_length)
                if(temp_pointer_length == pointer_length_max)
                    break;
            if(temp_pointer_length > pointer_length)
            {
                pointer_pos = temp_pointer_pos;
                pointer_length = temp_pointer_length;
                if(pointer_length == pointer_length_max)
                    break;
            }
        }
        coding_pos += pointer_length;
        if((coding_pos == uncompressed_size) && pointer_length)
        {
            output_pointer = (pointer_length == 1) ? 0 : ((pointer_pos << pointer_length_width) | (pointer_length - 2));
            output_lookahead_ref = coding_pos - 1;
        }
        else
        {
            output_pointer = (pointer_pos << pointer_length_width) | (pointer_length ? (pointer_length - 1) : 0);
            output_lookahead_ref = coding_pos;
        }
        *((uint16_t *) (compressed_text + compressed_pointer)) = output_pointer;
        compressed_pointer += 2;
        *(compressed_text + compressed_pointer++) = *(uncompressed_text + output_lookahead_ref);
        output_size += 3;
    }

    return output_size;
}

uint32_t lz77_decompress (uint8_t *compressed_text, uint8_t *uncompressed_text)
{

    uint8_t pointer_length_width;
    uint16_t input_pointer, pointer_length, pointer_pos, pointer_length_mask;
    uint32_t compressed_pointer, coding_pos, pointer_offset, uncompressed_size;

    /* uncompressed size is stored in first 4 bytes of compressed text, grab data */
    uncompressed_size = *((uint32_t *) compressed_text);

    /* skip these first 4 bytes on our pointer length width */
    pointer_length_width = *(compressed_text + 4);
    compressed_pointer = 5;

    pointer_length_mask = pow(2, pointer_length_width) - 1;

    /* we keep looping as long as our position is less than the final, uncompressed size */
    for(coding_pos = 0; coding_pos < uncompressed_size; ++coding_pos)
    {
        input_pointer = *((uint16_t *) (compressed_text + compressed_pointer));
        compressed_pointer += 2;
        pointer_pos = input_pointer >> pointer_length_width;
        pointer_length = pointer_pos ? ((input_pointer & pointer_length_mask) + 1) : 0;
        if(pointer_pos)
            for(pointer_offset = coding_pos - pointer_pos; pointer_length > 0; --pointer_length)
                uncompressed_text[coding_pos++] = uncompressed_text[pointer_offset++];
        *(uncompressed_text + coding_pos) = *(compressed_text + compressed_pointer++);
    }

    return coding_pos;
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


/*helper function to get size of a file by seeking to end of file and getting length of pos */
long fsize (FILE *in)
{
    long pos, length;
    pos = ftell(in);
    fseek(in, 0L, SEEK_END);
    length = ftell(in);
    fseek(in, pos, SEEK_SET);
    return length;
}

uint32_t file_lz77_compress (char *filename_in, char *filename_out, size_t malloc_size, uint8_t pointer_length_width)
{
    /* vars for use in method, *in and *out are the files, we have pointers for the texts themselves, and then sizes*/
    FILE *in, *out;
    uint8_t *uncompressed_text, *compressed_text;
    uint32_t uncompressed_size, compressed_size;


    /* try to open the file, if it fails, immediately return 0 */
    in = fopen(filename_in, "r");
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
    compressed_size = lz77_compress(uncompressed_text, uncompressed_size, compressed_text, pointer_length_width);

    /* once compressed, write out to file, if opening or writing fails return 0 */
    out = fopen(filename_out, "w");
    if(out == NULL)
        return 0;
    if((compressed_size != fwrite(compressed_text, 1, compressed_size, out)))
        return 0;
    fclose(out);

    /* on success return the new size of the compressed file */
    return compressed_size;
}

uint32_t file_lz77_decompress (char *filename_in, char *filename_out)
{
    /* vars for use in method, *in and *out are the files themselves, then the texts and sizes*/
    FILE *in, *out;
    uint32_t compressed_size, uncompressed_size;
    uint8_t *compressed_text, *uncompressed_text;

    /* try to open the compressed file, if fail return 0 */
    in = fopen(filename_in, "r");
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
    if(lz77_decompress(compressed_text, uncompressed_text) != uncompressed_size)
        return 0;

    /* then write to file checking for failing to open or write*/
    out = fopen(filename_out, "w");
    if(out == NULL)
        return 0;
    if(fwrite(uncompressed_text, 1, uncompressed_size, out) != uncompressed_size)
        return 0;
    fclose(out);

    /* return new, uncompressed size */
    return uncompressed_size;
}