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
#define MODE_NONE    0
#define MODE_ENCRYPT 1
#define MODE_DECRYPT 2

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
    char *outfile = NULL;
    char *passphrase = NULL;
    unsigned int rounds = ROUNDS_DEFAULT;
    int key[KEY_WORDS];
    size_t original_size = 0;
    BlockNode *list = NULL;
    FileHeader hdr;

    /* Auto-derived output name lives here when -o is omitted */
    char outfile_buf[OUTFILE_BUF];

    /* Parse arguments */
    if (parse_args(argc, argv, &mode, &infile, &outfile, &passphrase,
                   &rounds) != 0)
    {
        print_usage(argv[0]);
        return 1;
    }

    if (mode == MODE_NONE || infile == NULL || passphrase == NULL)
    {
        fprintf(stderr, "ERROR: -e/-d, -i, and -p are all required\n");
        print_usage(argv[0]);
        return 1;
    }

    /* Auto-set output name if -o was not given */
    if (outfile == NULL)
    {
        if (build_output_name(infile, (mode == MODE_ENCRYPT), outfile_buf,
                              OUTFILE_BUF) != 0)
        {
            fprintf(stderr, "ERROR: input filename too long to derive "
                            "output name -- please supply -o\n");
            return 1;
        }
        outfile = outfile_buf;
        printf("Output name not specified -- using '%s'\n", outfile);
    }

    /* Prevent in-place overwrite */
    if (files_are_same(infile, outfile))
    {
        fprintf(
            stderr,
            "ERROR: input and output filenames are the same ('%s')\n"
            "       This would corrupt the input. Use a different -o name.\n",
            infile);
        return 1;
    }

    /* ---- Derive key from passphrase ------------------------------------ */
    key_from_passphrase(passphrase, key);

#ifdef DEBUG
    fprintf(stderr, "[DEBUG] Mode: %s  Rounds: %u\n",
            (mode == MODE_ENCRYPT) ? "ENCRYPT" : "DECRYPT", rounds);
    debug_print_key(key);
#endif

    /* ---- Read input file into linked list ------------------------------ */
    list = file_to_list(infile, &original_size);
    if (list == NULL)
    {
        fprintf(stderr, "ERROR: could not read '%s'\n", infile);
        return 1;
    }

#ifdef DEBUG
    fprintf(stderr, "[DEBUG] Input list (%lu blocks):\n", 
        (unsigned long)list_length(list));
    debug_print_list(list);
#endif

    /* ==================================================================
     * ENCRYPT PATH
     * ==================================================================
     *  1. Generate data IV and encipher all data blocks in-place.
     *  2. Prepend data IV block.
     *  3. Build a FileHeader with original size and filename.
     *  4. header_encode() prepends a random header IV block and 10 CBC-encrypted
     *     metadata blocks. Final on-disk layout:
     *
     *       block  0    : header CBC IV (plaintext)
     *       block  1    : "XTEAFILE" magic  (encrypted)
     *       block  2    : original file size (encrypted)
     *       blocks 3-10 : original filename  (encrypted, null-padded)
     *       block  11   : data CBC IV (plaintext)
     *       blocks 12+  : file data          (encrypted)
     * ================================================================== */
    if (mode == MODE_ENCRYPT)
    {
        size_t total_bytes;
        int data_iv[2];
        BlockNode *data_iv_node;

        printf("Encrypting '%s' -> '%s'  (%u rounds) ...\n", infile, outfile,
               rounds);

        /* Seed PRNG from available variation to produce IVs */
        srand((unsigned int)((size_t)infile ^ original_size ^ rounds));

        /* Generate data IV and encipher all data blocks */
        random_iv(data_iv);
        list_encipher(list, rounds, key, data_iv);

        /* Prepend data IV block */
        data_iv_node = (BlockNode *)malloc(sizeof(BlockNode));
        if (data_iv_node == NULL) {
            fprintf(stderr, "ERROR: malloc failed for data IV\n");
            list_free(list);
            return 1;
        }
        data_iv_node->byte_block[0] = data_iv[0];
        data_iv_node->byte_block[1] = data_iv[1];
        data_iv_node->next_byte_block = list;
        list = data_iv_node;

        /* Populate metadata header */
        memset(&hdr, 0, sizeof(hdr));
        hdr.original_size = original_size;
        strncpy(hdr.filename, infile, HEADER_FNAME_BYTES - 1);
        hdr.filename[HEADER_FNAME_BYTES - 1] = '\0';

        /* Prepend the IV and 10 encrypted header blocks */
        if (header_encode(&list, &hdr, rounds, key) != 0)
        {
            fprintf(stderr, "ERROR: header_encode failed\n");
            list_free(list);
            return 1;
        }

#ifdef DEBUG
        fprintf(stderr, "[DEBUG] List after encryption (%lu blocks total):\n",
                (unsigned long)list_length(list));
        debug_print_list(list);
#endif

        /* Write all blocks -- header + data, always a multiple of 8 bytes */
        total_bytes = list_length(list) * BLOCK_BYTES;
        if (list_to_file(outfile, list, total_bytes) != 0)
        {
            fprintf(stderr, "ERROR: could not write '%s'\n", outfile);
            list_free(list);
            return 1;
        }

        printf("Done. Encrypted %lu bytes -> '%s'\n",
               (unsigned long)original_size, outfile);

        /* ==================================================================
         * DECRYPT PATH
         * ==================================================================
         *  1. header_decode() pops the IV + 10 encrypted header blocks,
         *     deciphers them in CBC mode, and checks the magic phrase
         *     "XTEAFILE".
         *       magic matches  -> key is correct, hdr is filled
         *       magic mismatch -> wrong passphrase (or wrong rounds) -- abort
         *  2. Decipher the remaining data blocks.
         *  3. Write exactly original_size bytes to strip zero-padding.
         *  4. Display the recovered metadata.
         * ================================================================== */
    }
    else
    {
        int decode_result;
        int data_iv[2];

        printf("Decrypting '%s' -> '%s'  (%u rounds) ...\n", infile, outfile,
               rounds);

        memset(&hdr, 0, sizeof(hdr));
        decode_result = header_decode(&list, &hdr, rounds, key);

        if (decode_result == -2)
        {
            fprintf(stderr,
                    "ERROR: file is too short to contain a valid XTEA header\n"
                    "       (minimum size is %d bytes)\n",
                    HEADER_TOTAL_BYTES);
            list_free(list);
            return 1;
        }
        if (decode_result == -1)
        {
            /*
             * Magic mismatch -- the most likely causes are:
             *   - wrong passphrase
             *   - different -r round count used at encryption time
             *   - file was not encrypted with this program
             */
            fprintf(stderr,
                    "ERROR: key verification failed -- wrong passphrase or\n"
                    "       round count (encrypted with %u rounds?)\n",
                    rounds);
            list_free(list);
            return 1;
        }

        /* Magic matched -- key confirmed correct */
        printf("Key verified OK\n");
        printf("Original filename : %s\n", hdr.filename);
        printf("Original size     : %lu bytes\n",
               (unsigned long)hdr.original_size);

        /* Pop data IV and decipher data blocks */
        if (pop_block(&list, data_iv) != 0) {
            fprintf(stderr, "ERROR: missing data IV block\n");
            list_free(list);
            return 1;
        }
        list_decipher(list, rounds, key, data_iv);

#ifdef DEBUG
        fprintf(stderr, "[DEBUG] List after decryption (%lu data blocks):\n",
                (unsigned long)list_length(list));
        debug_print_list(list);
#endif

        if (list_to_file(outfile, list, hdr.original_size) != 0)
        {
            fprintf(stderr, "ERROR: could not write '%s'\n", outfile);
            list_free(list);
            return 1;
        }

        printf("Done. Decrypted %lu bytes -> '%s'\n",
               (unsigned long)hdr.original_size, outfile);
    }

    printf("\n\n");
    list_free(list);
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
        if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--encrypt") == 0)
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
                fprintf(stderr, "ERROR: -p requires a passphrase\n");
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
    return 0;
}

/* =========================================================================
 * print_usage
 * ========================================================================= */
static void print_usage(const char *full_path)
{
    char filename[4096];
    extract_name(full_path, filename, sizeof(filename));

    printf("\nUsage:\n");
    printf("  %s -e -i <infile> [-o <outfile>] -p <passphrase> [-r <N>]\n",
           filename);
    printf("  %s -d -i <infile> [-o <outfile>] -p <passphrase> [-r <N>]\n",
           filename);
    printf("\nOptions:\n");
    printf("  -e            Encrypt\n");
    printf("  -d            Decrypt\n");
    printf("  -i <file>     Input file (any binary type)\n");
    printf(
        "  -o <file>     Output file (optional -- auto-derived if omitted)\n");
    printf("  -p <phrase>   Passphrase  (use quotes for spaces)\n");
    printf("  -r <N>        XTEA rounds (default 32; must match on decrypt)\n");
    printf("  -h            Show this help\n");
    printf("\nAuto-naming (when -o is omitted):\n");
    printf("  Encrypt:  photo.jpg      ->  photo.jpg.enc\n");
    printf("  Decrypt:  photo.jpg.enc  ->  photo.jpg\n");
    printf(
        "  Decrypt:  photo.jpg      ->  photo.jpg.dec  (no .enc to strip)\n\n");
}
