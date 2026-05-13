#include <stdio.h>
#include "fileproperty.h"
#define NO_FLAGS 1


int filesearch(char *argv[]){
    FILE *file = fopen(argv[1], "r");
    if(file != NULL){
        printf("This file does exist. \n");
        fclose(file);
        return 0;
    }
    else{
        printf("This file does not exist.\n");
        return 1;
    }
}

long filesize(char *argv[]){
    FILE *file = fopen(argv[1], "rb");
    if(file != NULL){
        if(fseek(file, 0L, SEEK_END) == 0){
            long filesizevalue = ftell(file);
            fclose(file);
            return filesizevalue;
        }
        else{
            return 0;
        }
    }
    else{
        return 0;
    }
}

/* Main() recieves file name as second argument vector. */
int main(int argc, char *argv[]){
    switch(argc){
        case 2:
            /* If file is found successfully, filesearch() returns 0. */
            if(filesearch(argv) == 0){
                long filesizevalue = filesize(argv);
                    /* filesize() returns 0 if an error occurred. */
                    if(filesizevalue > 0){
                    printf("This file is %ld bytes.", filesizevalue);
                }
            }
            break;
        /* If no file name was specified in argument vector. */
        case NO_FLAGS:
            printf("Please specify a file name.");
    }
return 0;
}
