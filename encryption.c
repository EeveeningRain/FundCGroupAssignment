#include "encryption.h"
#include "helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* preprocessor definitions */
#define DELTA 0x9E3779B9

/* XTEA algorithm derived from Wikipedia*/
/* take 64 bits of data in v[0] and v[1] and 128 bits of secret_key[0] -
 * secret_key[3] */

/* secret_key is 4 32-bit "words"*/

/* iteration_count should be 32, 64, 64 etc, but recommended value is 32 */

/* delta = 0x9E3779B9 is the golden ratio (φ) scaled to 32 bits.
 * Its purpose is to ensure that each round's sum value is unique,
 * preventing slide attacks — it's essentially a built-in round constant.
 */

/* Functions assume int size of 32 bits */
void encipher(unsigned int iteration_count, int byte_block[2],
              int const secret_key[4])
{
    unsigned int i;
    int byte_block_lower = byte_block[0], byte_block_upper = byte_block[1],
        sum = 0, delta = DELTA;
    for (i = 0; i < iteration_count; i++)
    {
        byte_block_lower +=
            (((byte_block_upper << 4) ^ (byte_block_upper >> 5)) +
             byte_block_upper) ^
            (sum + secret_key[sum & 3]);
        sum += delta;
        byte_block_upper +=
            (((byte_block_lower << 4) ^ (byte_block_lower >> 5)) +
             byte_block_lower) ^
            (sum + secret_key[(sum >> 11) & 3]);
    }
    byte_block[0] = byte_block_lower;
    byte_block[1] = byte_block_upper;
}

void decipher(unsigned int iteration_count, int byte_block[2],
              int const secret_key[4])
{
    unsigned int i;
    int byte_block_lower = byte_block[0], byte_block_upper = byte_block[1],
        delta = DELTA, sum = delta * iteration_count;
    for (i = 0; i < iteration_count; i++)
    {
        byte_block_upper -=
            (((byte_block_lower << 4) ^ (byte_block_lower >> 5)) +
             byte_block_lower) ^
            (sum + secret_key[(sum >> 11) & 3]);
        sum -= delta;
        byte_block_lower -=
            (((byte_block_upper << 4) ^ (byte_block_upper >> 5)) +
             byte_block_upper) ^
            (sum + secret_key[sum & 3]);
    }
    byte_block[0] = byte_block_lower;
    byte_block[1] = byte_block_upper;
}

int do_encryption(const int mode, const char *infile, const char *outfile,
                  const char *passphrase, const unsigned int rounds)
{

    int key[KEY_WORDS];
    size_t original_size = 0;
    BlockNode *list = NULL;
    FileHeader hdr;

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

    /* ---- Derive key from passphrase ------------------------------------ */
    key_from_passphrase(passphrase, key);

#ifdef DEBUG
    fprintf(stderr, "[DEBUG] Mode: %s  Rounds: %u\n",
            (mode == MODE_ENCRYPT) ? "ENCRYPT" : "DECRYPT", rounds);
    debug_print_key(key);
#endif

    /* ==================================================================
     * ENCRYPT PATH
     * ==================================================================
     *  1. Generate data IV and encipher all data blocks in-place.
     *  2. Prepend data IV block.
     *  3. Build a FileHeader with original size and filename.
     *  4. header_encode() prepends a random header IV block and 10
     * CBC-encrypted metadata blocks. Final on-disk layout:
     *
     *       block  0    : header CBC IV (plaintext)
     *       block  1    : "XTEAFILE" magic  (encrypted)
     *       block  2    : original file size (encrypted)
     *       blocks 3-10 : original filename  (encrypted, null-padded)
     *       block  11   : data CBC IV (plaintext)
     *       blocks 12+  : file data          (encrypted)
     * ================================================================== */
    if (mode == 1 || mode == 5) /* Mode: Encrypt */
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
        if (data_iv_node == NULL)
        {
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
    }

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
    else if (mode == 2 || mode == 6) /* Mode == Decrypt */
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
        if (pop_block(&list, data_iv) != 0)
        {
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
