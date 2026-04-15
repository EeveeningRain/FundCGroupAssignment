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
    return 1;
}

int encrypt_file(FILE* file){
    return 0;
}

int deencrypt_file(FILE* file){
    return 0;
}

int compress_file(FILE* file){
    return 0;
}

int decompress_file(FILE* file){
    return 0;
}
