#include "securedisk.h"
#include "helpers.h"
#include "compress.c"
#include "encryption.h"

/* having some kind of array storing files to make the disk? */
/*sorting algos, search algos? */
/* user needs to be able to find a file on their directory, which we then work on and store in our own dir*/
/*we'll need runtime arguments; see curr example in main*/

int main(int argc, char *argv[]){
    /*if(argc > 1){
        printf("hi!\n");
        if(strcmp(argv[1], "c")){
            printf("wow\n");
        }*/

    return 0;
}

int encrypt_file(FILE* file){
    return 0;
}

int deencrypt_file(FILE* file){
    return 0;
}



int compress_file(FILE* file, char* filename, int bitsize){
    if(file == NULL)
        return 0;
    printf("Original size: %ld\n", fsize(file));
    fclose(file);

    char* filename_out = malloc(50 * sizeof(char));
    filename_out[0] = '\0';
    strcat(filename_out, "files/");
    strcpy(filename_out, filename);
    strcat(filename_out, ".lz77\0");

    uint8_t i = 1;
    for(; i < bitsize; ++i)
        printf("Compressed (%i): %u\n", i, file_lz77_compress(filename, filename_out, 100000000, i));

    free(filename_out);
    return 0;
}

int decompress_file(FILE* file, char* filename){
    if(file == NULL)
        return 0;
    printf("Original size: %ld\n", fsize(file));
    fclose(file);

    char* filename_out = malloc(50 * sizeof(char));
    printf("Decompressed: (%u)\n", file_lz77_decompress(filename, filename_out));

    free(filename_out);
    return 0;
}



/*testing compression:
    char* input = malloc(20 * sizeof(char));
    printf("please enter filename to compress: ");
    scanf("%s", input);
    int bitsize = 1;
    printf("please enter bit size: ");
    scanf("%d", &bitsize);

    compress_file(fopen(input, "r"), input, bitsize);

    printf("please enter compressed file to uncompress: ");
    scanf("%s", input);

    decompress_file(fopen(input, "r"), input);

*/