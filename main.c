/*
 * main.c
 * ------
 * Group: Wed_6pm_07
 *
 * file compression, encryption, and management tool.
 *
 * Compile (normal):    make
 * Compile (debug):     make debug
 *
 * Usage:
 *   ./<exe name> -e -i <infile> [-o <outfile>] -p <passphrase> [-r <rounds>]
 *   ./<exe name> -d -i <infile> [-o <outfile>] -p <passphrase> [-r <rounds>]
 *
 * If -o is omitted:
 *   encrypt: output is <infile>.enc
 *   decrypt: output is <infile> with .enc stripped
 *
 * Options:
 *   -e            Encrypt mode
 *   -d            Decrypt mode
 *   -i <file>     Input file (any binary type)
 *   -o <file>     Output file (optional - auto-derived if omitted)
 *   -p <phrase>   Passphrase used to derive the 128-bit key
 *   -r <rounds>   XTEA encryption rounds (default: 32)
 *   -h            Print this help and exit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "encryption.h"
#include "compress.h"
#include "helpers.h"

/* -------------------------------------------------------------------------
 * Forward declarations
 * ---------------------------------------------------------------------- */
static void print_usage(const char *prog);
static int parse_args(int argc, char *argv[], int *mode, char **infile,
                      char **outfile, char **passphrase, unsigned int *rounds);

/* -------------------------------------------------------------------------
 * Preprocessor definitions
 * ---------------------------------------------------------------------- */
#define MODE_NONE       0
#define MODE_ENCRYPT    1
#define MODE_DECRYPT    2
#define MODE_COMPRESS   3
#define MODE_UNCOMPRESS 4
#define MODE_COMPRESS_AND_ENCRYPT   5
#define MODE_DECRYPT_AND_UNCOMPRESS 6

/* Buffer size for auto-derived output filenames */
#define OUTFILE_BUF 4096

#define ROUNDS_DEFAULT 32

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{

    int mode = MODE_NONE;
    char *infile = NULL;
    char *tempfile = "tempfile.tmp";
    char *outfile = NULL;
    char *passphrase = NULL;
    unsigned int rounds = ROUNDS_DEFAULT;

    /* Parse arguments */
    if (parse_args(argc, argv, &mode, &infile, &outfile, &passphrase,
                   &rounds) != 0)
    {
        print_usage(argv[0]);
        return 1;
    }

    if (mode == MODE_ENCRYPT || mode == MODE_DECRYPT)
    {
        do_encryption(mode, infile, outfile, passphrase, rounds);
    }

    if(mode == MODE_COMPRESS || mode == MODE_UNCOMPRESS){
        do_compression(mode, infile, outfile);
    }

    if(mode == MODE_COMPRESS_AND_ENCRYPT){
        do_compression(mode, infile, tempfile);
        do_encryption(mode, tempfile, outfile, passphrase, rounds);
        remove(tempfile);
    }

    if(mode == MODE_DECRYPT_AND_UNCOMPRESS){
        do_encryption(mode, infile, tempfile, passphrase, rounds);
        do_compression(mode, tempfile, outfile);
        remove(tempfile);
    }

    return 0;
}

/* =========================================================================
 * parse_args
 * =========================================================================
 * Handles: -e  -d  -i <file>  -o <file>  -p <phrase>  -r <N>  -h
 * Returns 0 on success, -1 on error or -h.
 */
static int parse_args(int argc, char *argv[], int *mode, char **infile,
                      char **outfile, char **passphrase, unsigned int *rounds)
{
    int i;
    for (i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compress") == 0){
            if (*mode == MODE_UNCOMPRESS)
            {
                fprintf(stderr, "ERROR: cannot specify both -c and -u\n");
                return -1;
            }
            *mode = MODE_COMPRESS;
        }
        else if(strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--uncompress") == 0){
            if (*mode == MODE_COMPRESS)
            {
                fprintf(stderr, "ERROR: cannot specify both -c and -u\n");
                return -1;
            }
            *mode = MODE_UNCOMPRESS;
        }
        else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--encrypt") == 0)
        {
            if (*mode == MODE_DECRYPT)
            {
                fprintf(stderr, "ERROR: cannot specify both -e and -d\n");
                return -1;
            }
            *mode = MODE_ENCRYPT;
        }
        else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--decrypt") == 0)
        {
            if (*mode == MODE_ENCRYPT)
            {
                fprintf(stderr, "ERROR: cannot specify both -e and -d\n");
                return -1;
            }
            *mode = MODE_DECRYPT;
        }
        else if(strcmp(argv[i], "-z") == 0 || strcmp(argv[i], "--compressandencrypt") == 0){
            if (*mode != MODE_NONE){
                fprintf(stderr, "ERROR: cannot specify -z with other mode flags!\n");
            }
            *mode = MODE_COMPRESS_AND_ENCRYPT;
        }
        else if(strcmp(argv[i], "-x") == 0 || strcmp(argv[i], "--decryptanduncompress") == 0){
            if (*mode != MODE_NONE){
                fprintf(stderr, "ERROR: cannot specify -x with other mode flags!\n");
            }
            *mode = MODE_DECRYPT_AND_UNCOMPRESS;
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            return -1;
        }
        else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--input") == 0)
        {
            if (++i >= argc)
            {
                fprintf(stderr, "ERROR: -i requires a filename\n");
                return -1;
            }

            if (argv[i][0] == '-')
            {
                fprintf(stderr, "WARNING: unexpected argument tag'%s'\n"
                                "         Are you sure you entered the file "
                                "name?\n", argv[i]);
            }
            *infile = argv[i];
        }
        else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0)
        {
            if (++i >= argc)
            {
                fprintf(stderr, "ERROR: -o requires a filename\n");
                return -1;
            }

            if (argv[i][0] == '-')
            {
                fprintf(stderr, "WARNING: unexpected argument tag'%s'\n"
                                "         Are you sure you entered the file "
                                "name?\n", argv[i]);
            }
            *outfile = argv[i];
        }
        else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--passphrase") == 0)
        {
            if (++i >= argc)
            {
                fprintf(stderr, "ERROR: -p requires a passphrase to be entered\n");
                return -1;
            }

            if (argv[i][0] == '-')
            {
                fprintf(stderr, "WARNING: unexpected argument '%s'\n"
                                "         Are you sure you entered the "
                                "password?\n", argv[i]);
            }
            *passphrase = argv[i];
        }
        else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--rounds") == 0)
        {
            if (++i >= argc)
            {
                fprintf(stderr, "ERROR: -r requires a number\n");
                return -1;
            }
            if (argv[i][0] == '-')
            {
                fprintf(stderr, "ERROR: -r requires a number\n");
                return -1;
            }
            *rounds = (unsigned int)atoi(argv[i]);
            if (*rounds == 0)
            {
                fprintf(stderr, "ERROR: rounds must be > 0\n");
                return -1;
            }
            if ((*rounds & (*rounds - 1)) != 0)
            {
                fprintf(stderr, "ERROR: rounds must be a power of 2\n");
                return -1;
            }
        }
        else
        {
            fprintf(stderr, "ERROR: unknown option '%s'\n", argv[i]);
            return -1;
        }
    }

    /*
     * Error Handling
     */

    /* Mode not defined */
    if (*mode == MODE_NONE)
    {
        fprintf(stderr, "ERROR: Mode must be selected\n");
        print_usage(argv[0]);
        return -1;
    }

    /* Input file not defined */
    if (*infile == NULL)
    {
        fprintf(stderr, "ERROR: -i flag (input file) must be defined\n");
        print_usage(argv[0]);
        return -1;
    }

    /* Passphrase not defined when trying to use cipher */
    if ((*mode == MODE_ENCRYPT || *mode == MODE_DECRYPT) && *passphrase == NULL)
    {
        fprintf(stderr, "ERROR:  -p flag (passphrase) is required when using encrypt or decrypt\n");
        print_usage(argv[0]);
        return -1;
    }

    /* Auto-set output name if -o was not given */
    if (*outfile == NULL)
    {
        /* Auto-derived output name lives here when -o is omitted */
        char *outfile_buf = malloc(OUTFILE_BUF);
        if (!outfile_buf) return -1;

        if (build_output_name(*infile, (*mode == MODE_ENCRYPT), outfile_buf,
                              OUTFILE_BUF) != 0)
        {
            fprintf(stderr, "ERROR: input filename too long to derive "
                            "output name -- please supply -o\n");
            return -1;
        }
        *outfile = outfile_buf;
        printf("Output name not specified -- using '%s'\n", *outfile);
    }

    /* Prevent in-place overwrite */
    if (files_are_same(*infile, *outfile))
    {
        fprintf(
            stderr,
            "ERROR: input and output filenames are the same ('%s')\n"
            "       This would corrupt the input. Use a different -o name.\n",
            *infile);
        return -1;
    }

    return 0;
}

/* *************************************************************************
 * print_usage
 * ************************************************************************* */
static void print_usage(const char *full_path)
{
    char filename[4096];
    extract_name(full_path, filename, sizeof(filename));

    printf("\nUsage:\n");
    printf("  %s -e -i <infile> [-o <outfile>] -p <passphrase> [-r <N>]\n",
           filename);
    printf("  %s -d -i <infile> [-o <outfile>] -p <passphrase> [-r <N>]\n",
           filename);
    printf("  %s -s -i <infile> [-o <outfile>] -p <passphrase> [-r <N>]\n",
           filename);
    printf("\nOptions:\n");
    printf("  -c,            --compress               Compress\n");
    printf("  -u,            --uncompress             Uncompress\n");
    printf("  -e,            --encrypt                Encrypt\n");
    printf("  -d,            --decrypt                Decrypt\n");
    printf("  -z,            --compressandencrypt     Compress & Encrypt\n");
    printf("  -x             --decryptanduncompress   Decrypt & Uncompress\n");
    printf("  -i <file>,     --input <file>           Input file (any binary type)\n");
    printf("  -o <file>,     --output <file>          Output file (optional -- auto-derived if omitted)\n");
    printf("  -p <phrase>,   --passphrase <phrase>    Passphrase  (use quotes for spaces)\n");
    printf("  -r <num>,      --rounds <num>           XTEA rounds (default 32; must match on decrypt)\n");
    printf("  -h,            --help                   Show this help\n");
    printf("\nAuto-naming (when -o is omitted):\n"); 
    printf("  Compress:      text.txt       ->  text.txt.lz77\n");
    printf("  Uncompress:    text.txt.lz77  ->  text.txt\n");
    printf("  Encrypt:       photo.jpg      ->  photo.jpg.enc\n");
    printf("  Decrypt:       photo.jpg.enc  ->  photo.jpg\n");
    printf("  Decrypt:       photo.jpg      ->  photo.jpg.dec  (no .enc to strip)\n\n");
    printf("  Compress & Encrypt:   text.txt          -> text.txt.lz77.enc\n");
    printf("  Decrypt & Uncompress: text.txt.lz77.enc -> text.txt\n");
}
