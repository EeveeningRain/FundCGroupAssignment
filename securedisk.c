#include "securedisk.h"
#include "helpers.h"

/* having some kind of array storing files to make the disk? */
/*sorting algos, search algos? */
/* user needs to be able to find a file on their directory, which we then work on and store in our own dir*/
/*we'll need runtime arguments; see curr example in main*/

int main(int argc, char *argv[]){
    if(argc > 1){
        printf("hi!\n");
        if(strcmp(argv[1], "c")){
            printf("wow\n");
        }
    }
    /*get in some kind of file?*/
    /*
     * compress
     * encrypt
     *
     * ?
     *
     * deenrypt,
     * decompress */
    return 0;
}

int encrypt_file(FILE* file){
    return 0;
}

int deencrypt_file(FILE* file){
    return 0;
}


/*
LZW ENCODING
  *     PSEUDOCODE
  1     Initialize table with single character strings
  2     P = first input character
  3     WHILE not end of input stream
  4          C = next input character
  5          IF P + C is in the string table
  6            P = P + C
  7          ELSE
  8            output the code for P
  9          add P + C to the string table
  10           P = C
  11         END WHILE
  12    output code for P 

*/
int compress_file(FILE* file){
    return 0;
}

int decompress_file(FILE* file){
    return 0;
}
