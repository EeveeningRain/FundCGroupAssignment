#include "helpers.h"
#include "encryption.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * LINKED-LIST OPERATIONS
 * ========================================================================= */

/*
 * list_append
 * -----------
 * Allocates a new BlockNode, fills it with (byte_block_lower,
 * byte_block_upper), and appends it to the list tracked by *head and *tail.
 * Both pointers are updated as needed.
 *
 * Returns a pointer to the new node, or NULL if malloc fails.
 */
BlockNode *list_append(BlockNode **head, BlockNode **tail, int byte_block_lower,
                       int byte_block_upper)
{
    BlockNode *node = (BlockNode *)malloc(sizeof(BlockNode));
    if (node == NULL)
    {
        fprintf(stderr, "ERROR: list_append - malloc failed\n");
        return NULL;
    }

    node->byte_block[0] = byte_block_lower;
    node->byte_block[1] = byte_block_upper;
    node->next_byte_block = NULL;

    if (*head == NULL)
    {
        /* First node - both head and tail point to it */
        *head = node;
        *tail = node;
    }
    else
    {
        /* Chain onto existing tail, then advance tail */
        (*tail)->next_byte_block = node;
        *tail = node;
    }

    return node;
}

/*
 * list_free
 * ---------
 * Walks the list and frees every node.
 */
void list_free(BlockNode *head)
{
    BlockNode *current_byte_block = head;
    BlockNode *next_byte_block;
    while (current_byte_block != NULL)
    {
        next_byte_block = current_byte_block->next_byte_block;
        free(current_byte_block);
        current_byte_block = next_byte_block;
    }
}

/*
 * list_length
 * -----------
 * Returns the number of nodes in the list.
 */
size_t list_length(const BlockNode *head)
{
    size_t count = 0;
    const BlockNode *current_byte_block = head;
    while (current_byte_block != NULL)
    {
        count++;
        current_byte_block = current_byte_block->next_byte_block;
    }
    return count;
}

/* =========================================================================
 * BYTE  <->  BLOCK CONVERSION
 * ========================================================================= */

/*
 * pack_block
 * ----------
 * Packs 8 bytes into an int[2] using big-endian order.
 * bytes[0..3] -> byte_block[0],  bytes[4..7] -> byte_block[1]
 *
 * Big-endian is used throughout so that encrypted files produced on one
 * machine can be decrypted on any other machine regardless of native byte
 * order (e.g. x86 little-endian vs ARM big-endian).
 */
void pack_block(int byte_block[2], const unsigned char bytes[BLOCK_BYTES])
{
    byte_block[0] = ((unsigned int)bytes[0] << 24) |
                    ((unsigned int)bytes[1] << 16) |
                    ((unsigned int)bytes[2] << 8) | (unsigned int)bytes[3];

    byte_block[1] = ((unsigned int)bytes[4] << 24) |
                    ((unsigned int)bytes[5] << 16) |
                    ((unsigned int)bytes[6] << 8) | (unsigned int)bytes[7];
}

/*
 * unpack_block
 * ------------
 * Reverses pack_block: extracts bytes from int[2] in big-endian order.
 */
void unpack_block(const int byte_block[2], unsigned char bytes[BLOCK_BYTES])
{
    bytes[0] = (unsigned char)((byte_block[0] >> 24) & 0xFF);
    bytes[1] = (unsigned char)((byte_block[0] >> 16) & 0xFF);
    bytes[2] = (unsigned char)((byte_block[0] >> 8) & 0xFF);
    bytes[3] = (unsigned char)(byte_block[0] & 0xFF);

    bytes[4] = (unsigned char)((byte_block[1] >> 24) & 0xFF);
    bytes[5] = (unsigned char)((byte_block[1] >> 16) & 0xFF);
    bytes[6] = (unsigned char)((byte_block[1] >> 8) & 0xFF);
    bytes[7] = (unsigned char)(byte_block[1] & 0xFF);
}

/* =========================================================================
 * FILENAME UTILITIES
 * ========================================================================= */

/*
 * basename_only
 * -------------
 * returns a pointer to the filename component of a path
 * (the part after the last '/' or '\').  Returns the original pointer if
 * no separator is found.
 */
static const char *basename_only(const char *path)
{
    const char *p = path;
    const char *last = path;
    while (*p != '\0')
    {
        if (*p == '/' || *p == '\\')
            last = p + 1;
        p++;
    }
    return last;
}

/*
 * build_output_name
 * -----------------
 * Derives an output filename from the input filename when -o is not given.
 *
 * Encrypt mode:  appends ".enc"
 *   photo.jpg        ->  photo.jpg.enc
 *
 * Decrypt mode:  strips ".enc" suffix if present, otherwise appends ".dec"
 *   photo.jpg.enc    ->  photo.jpg
 *   photo.jpg        ->  photo.jpg.dec  (no .enc to strip - avoid overwrite)
 *
 * Returns 0 on success, -1 if result would overflow out_buf_len.
 */
int build_output_name(const char *infile, int mode, char *out_buf,
                      size_t out_buf_len)
{
    size_t in_len = strlen(infile);

    switch (mode)
    {
        case 0:
            printf("Mode is 0 (none)! This should not happen if parse_args is correct.\n");
            return -1;

        case 5:
            /* compress/encrypt, carry out next two options */

        case 3:
            if (in_len + 4 + 1 > out_buf_len)
            return -1;
            memcpy(out_buf, infile, in_len);
            memcpy(out_buf + in_len, ".lz77", 6); /* includes null */
            if (mode != 5) break; /* if just compress, we are done */

        case 1:
            /* append ".enc" */
            if (in_len + 4 + 1 > out_buf_len)
                return -1;
            memcpy(out_buf, infile, in_len);
            memcpy(out_buf + in_len, ".enc", 5); /* includes null */
            break;

        case 6:
            /* decrypt/uncompress, carry out next two options */

        case 4:
            /* strip ".lz77" if present */
            const char *lz77_suffix = ".lz77";
            size_t lz77_len = 5;
            if (in_len > lz77_len &&
                strcmp(infile + in_len - lz77_len, lz77_suffix) == 0)
            {
                /* strip the suffix */
                size_t new_len = in_len - lz77_len;
                if (new_len + 1 > out_buf_len)
                    return -1;
                memcpy(out_buf, infile, new_len);
                out_buf[new_len] = '\0';
            }
            else
            {
                /* no .enc suffix - append ".dec" to avoid overwriting input */
                if (in_len + 4 + 1 > out_buf_len)
                    return -1;
                memcpy(out_buf, infile, in_len);
                memcpy(out_buf + in_len, ".lz77u", 7);
            }

            if (mode != 6) break; /* if just uncompress, we are done */

        case 2:
            /* strip ".enc" if present */
            const char *enc_suffix = ".enc";
            size_t enc_len = 4;
            if (in_len > enc_len &&
                strcmp(infile + in_len - enc_len, enc_suffix) == 0)
            {
                /* strip the suffix */
                size_t new_len = in_len - enc_len;
                if (new_len + 1 > out_buf_len)
                    return -1;
                memcpy(out_buf, infile, new_len);
                out_buf[new_len] = '\0';
            }
            else
            {
                /* no .enc suffix - append ".dec" to avoid overwriting input */
                if (in_len + 4 + 1 > out_buf_len)
                    return -1;
                memcpy(out_buf, infile, in_len);
                memcpy(out_buf + in_len, ".dec", 5);
            }

    }

    return 0;
}

/*
 * files_are_same
 * --------------
 * Returns 1 if infile and outfile resolve to the same path, 0 otherwise.
 * Simple string comparison - sufficient when both names come from argv.
 */
int files_are_same(const char *a, const char *b)
{
    return (strcmp(a, b) == 0) ? 1 : 0;
}

/*
 * extract_name
 * -------------
 * Extracts the filename from a full path, excluding the directory and
 * extension.
 */
void extract_name(const char *path, char *out, size_t out_size)
{
    const char *start;
    const char *end;
    size_t len;

    /* Find last backslash */
    start = strrchr(path, '\\');
    if (start != NULL)
    {
        start++; /* move past '\' */
    }
    else
    {
        start = path; /* no path, just filename */
    }

    /* Find last dot */
    end = strrchr(start, '.');
    if (end == NULL)
    {
        end = start + strlen(start); /* no extension */
    }

    /* Calculate length safely */
    len = (size_t)(end - start);

    if (len >= out_size)
    {
        len = out_size - 1; /* prevent overflow */
    }

    memcpy(out, start, len);
    out[len] = '\0';
}

/* =========================================================================
 * CBC (CIPHER BLOCK CHAINING) HELPERS
 * ========================================================================= */

static void xor_block(int byte_block[2], const int prev_block[2])
{
    byte_block[0] ^= prev_block[0];
    byte_block[1] ^= prev_block[1];
}

static void copy_block(int dst[2], const int src[2])
{
    dst[0] = src[0];
    dst[1] = src[1];
}

void random_iv(int iv[2])
{
    iv[0] = ((rand() & 0xFFFF) << 16) | (rand() & 0xFFFF);
    iv[1] = ((rand() & 0xFFFF) << 16) | (rand() & 0xFFFF);
}

int pop_block(BlockNode **head, int cipher_block[2])
{
    BlockNode *node;

    if (*head == NULL)
        return -1;

    node = *head;
    *head = node->next_byte_block;
    cipher_block[0] = node->byte_block[0];
    cipher_block[1] = node->byte_block[1];
    free(node);
    return 0;
}

int read_block(BlockNode **node, int cipher_block[2])
{

    if (*node == NULL)
        return -1;

    cipher_block[0] = (*node)->byte_block[0];
    cipher_block[1] = (*node)->byte_block[1];
    return 0;
}

/* =========================================================================
 * KEY HELPERS
 * ========================================================================= */

/*
 * pack_key
 * --------
 * Packs 16 raw key bytes into four 32-bit words, big-endian.
 * Used if the caller supplies a binary key (e.g. from a key file).
 */
void pack_key(const unsigned char raw[KEY_BYTES], int key[KEY_WORDS])
{
    int i;
    for (i = 0; i < KEY_WORDS; i++)
    {
        key[i] = ((unsigned int)raw[i * 4] << 24) |
                 ((unsigned int)raw[i * 4 + 1] << 16) |
                 ((unsigned int)raw[i * 4 + 2] << 8) |
                 (unsigned int)raw[i * 4 + 3];
    }
}

/*
 * key_from_passphrase
 * -------------------
 * Derives a 128-bit key from a null-terminated passphrase string.
 *
 * Algorithm: djb2 hash seeded differently for each of the four key words,
 * so a short passphrase still fills all 128 bits with distinct values.
 * Each word uses a different prime seed to ensure the words differ even
 * when the passphrase is very short.
 *
 * Note: this is intentionally simple to stay within the allowed library
 * subset (no <stdint.h>, no crypto libraries).  Real applications would
 * use PBKDF2 or Argon2.
 */
void key_from_passphrase(const char *passphrase, int key[KEY_WORDS])
{
    /* Four prime seeds - one per key word */
    unsigned int seeds[KEY_WORDS] = {5381u, 52711u, 3145739u, 805306457u};
    int w, i;
    size_t len = strlen(passphrase);

    for (w = 0; w < KEY_WORDS; w++)
    {
        unsigned int hash = seeds[w];
        for (i = 0; i < (int)len; i++)
        {
            /* hash = hash * 33 XOR char  (classic djb2 body) */
            hash = ((hash << 5) + hash) ^ (unsigned int)passphrase[i];
        }
        key[w] = (int)hash;
    }
}

/* =========================================================================
 * FILE  ->  LINKED LIST
 * ========================================================================= */

/*
 * file_to_list
 * ------------
 * Opens 'filename' in binary read mode and reads its entire contents into a
 * newly-allocated linked list of BlockNodes.
 *
 * Padding:
 *   XTEA requires 8-byte blocks.  If the file length is not a multiple of 8,
 *   the final block is zero-padded.  *original_size is set to the true byte
 *   count so that list_to_file can strip the padding after decryption.
 *
 * Returns:
 *   Head of the block list on success.
 *   NULL on any error (file not found, malloc failure, empty file).
 */
BlockNode *file_to_list(const char *filename, size_t *original_size)
{
    FILE *fp = NULL;
    BlockNode *head = NULL;
    BlockNode *tail = NULL;
    unsigned char buf[BLOCK_BYTES];
    size_t bytes_read;
    size_t total = 0;

    fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        fprintf(stderr, "ERROR: file_to_list - cannot open '%s'\n", filename);
        return NULL;
    }

    /*
     * Read BLOCK_BYTES at a time.  fread returns the number of items read.
     * When it returns fewer than BLOCK_BYTES we've hit EOF (or an error).
     * We zero-fill the remainder of buf to create the padded final block.
     */
    while (1)
    {
        bytes_read = fread(buf, 1, BLOCK_BYTES, fp);

        if (bytes_read == 0)
        {
            /* Clean EOF with no leftover bytes - done */
            break;
        }

        total += bytes_read;

        if (bytes_read < BLOCK_BYTES)
        {
            /* Partial final block - zero-pad to 8 bytes */
            memset(buf + bytes_read, 0, BLOCK_BYTES - bytes_read);
        }

        /* Pack the 8 bytes into two ints and append to list */
        {
            int byte_block[2];
            pack_block(byte_block, buf);
            if (list_append(&head, &tail, byte_block[0], byte_block[1]) == NULL)
            {
                /* malloc failed mid-way - clean up and abort */
                list_free(head);
                fclose(fp);
                return NULL;
            }

#ifdef DEBUG
            debug_print_block("READ", byte_block[0], byte_block[1]);
#endif
        }

        if (bytes_read < BLOCK_BYTES)
        {
            /* Was the partial final block - stop reading */
            break;
        }
    }

    fclose(fp);

    if (head == NULL)
    {
        fprintf(stderr, "ERROR: file_to_list - file '%s' is empty\n", filename);
        return NULL;
    }

    *original_size = total;

#ifdef DEBUG
    fprintf(stderr, "[DEBUG] file_to_list: read %lu bytes, %lu blocks\n",
            (unsigned long)total, (unsigned long)list_length(head));
#endif

    return head;
}

/* =========================================================================
 * LINKED LIST  ->  FILE
 * ========================================================================= */

/*
 * list_to_file
 * ------------
 * Writes the block list out to 'filename' in binary mode, stopping after
 * exactly 'original_size' bytes to strip any zero-padding added during
 * encryption.
 *
 * Returns 0 on success, -1 on error.
 */
int list_to_file(const char *filename, const BlockNode *head,
                 size_t original_size)
{
    FILE *fp = NULL;
    const BlockNode *current_byte_block = head;
    size_t written = 0;
    unsigned char buf[BLOCK_BYTES];

    fp = fopen(filename, "wb");
    if (fp == NULL)
    {
        fprintf(stderr, "ERROR: list_to_file - cannot open '%s'\n", filename);
        return -1;
    }

    while (current_byte_block != NULL && written < original_size)
    {
        size_t remaining = original_size - written;
        size_t to_write = (remaining < BLOCK_BYTES) ? remaining : BLOCK_BYTES;

        unpack_block(current_byte_block->byte_block, buf);

        if (fwrite(buf, 1, to_write, fp) != to_write)
        {
            fprintf(stderr, "ERROR: list_to_file - fwrite failed\n");
            fclose(fp);
            return -1;
        }

#ifdef DEBUG
        debug_print_block("WRITE", current_byte_block->byte_block[0],
                          current_byte_block->byte_block[1]);
#endif

        written += to_write;
        current_byte_block = current_byte_block->next_byte_block;
    }

    fclose(fp);

#ifdef DEBUG
    fprintf(stderr, "[DEBUG] list_to_file: wrote %lu bytes to '%s'\n",
            (unsigned long)written, filename);
#endif

    return 0;
}

/* =========================================================================
 * ENCRYPT / DECRYPT  an entire linked list
 * ========================================================================= */

/*
 * list_encipher
 * -------------
 * Walks the list and encrypts every block in-place using CBC.
 * The first block is XORed with the provided IV, then encrypted. Each
 * subsequent block is XORed with the previous ciphertext block.
 */
void list_encipher(BlockNode *head, unsigned int rounds,
                   int const key[KEY_WORDS], int iv[2])
{
    BlockNode *current_byte_block = head;
    int prev_cipher[2];
    int byte_block[2];

    copy_block(prev_cipher, iv);

    while (current_byte_block != NULL)
    {
        byte_block[0] = current_byte_block->byte_block[0];
        byte_block[1] = current_byte_block->byte_block[1];
        xor_block(byte_block, prev_cipher);
        encipher(rounds, byte_block, key);
        copy_block(current_byte_block->byte_block, byte_block);
        copy_block(prev_cipher, byte_block);

#ifdef DEBUG
        debug_print_block("ENCIPHERED", current_byte_block->byte_block[0],
                          current_byte_block->byte_block[1]);
#endif

        current_byte_block = current_byte_block->next_byte_block;
    }
}

/*
 * list_decipher
 * -------------
 * Walks the list and decrypts every block in-place using CBC.
 * The first block is decrypted and XORed with the provided IV. Each
 * subsequent block is XORed with the previous ciphertext block.
 */
void list_decipher(BlockNode *head, unsigned int rounds,
                   int const key[KEY_WORDS], int iv[2])
{
    BlockNode *current_byte_block = head;
    int prev_cipher[2];
    int cipher_block[2];
    int plain_block[2];

    copy_block(prev_cipher, iv);

    while (current_byte_block != NULL)
    {
        cipher_block[0] = current_byte_block->byte_block[0];
        cipher_block[1] = current_byte_block->byte_block[1];
        plain_block[0] = cipher_block[0];
        plain_block[1] = cipher_block[1];

        decipher(rounds, plain_block, key);
        xor_block(plain_block, prev_cipher);
        copy_block(current_byte_block->byte_block, plain_block);
        copy_block(prev_cipher, cipher_block);

#ifdef DEBUG
        debug_print_block("DECIPHERED", current_byte_block->byte_block[0],
                          current_byte_block->byte_block[1]);
#endif

        current_byte_block = current_byte_block->next_byte_block;
    }
}

/* =========================================================================
 * HEADER ENCODE / DECODE
 * =========================================================================
 *
 * Header layout:
 *
 *  Block  0        iv         random CBC IV, stored plaintext
 *  Block  1        magic      "XTEAFILE"
 *  Block  2        size       original_size split into upper/lower 32-bit
 *  Blocks 3-10     filename   64 bytes, null-padded
 *
 * The IV is stored plaintext, and blocks 1..10 are encrypted in CBC mode.
 * The header is prepended to encrypted file data so the final order is
 * IV | encrypted header | encrypted data.
 * ========================================================================= */

/*
 * header_encode
 * -------------
 * Builds and prepends all 11 header blocks to *head.
 *
 * Blocks are prepended in REVERSE order of the final layout so that when
 * the last prepend (IV) completes, the list starts with the IV block.
 *
 *   Prepend order: filename → size → magic → iv
 *   Final order in file: iv | magic | size | filename | data...
 *
 * Returns 0 on success, -1 on malloc failure.
 */
int header_encode(BlockNode **head, const FileHeader *hdr, unsigned int rounds,
                  int const key[KEY_WORDS])
{
    unsigned char buf[BLOCK_BYTES];
    int header_blocks[HEADER_TOTAL_BLOCKS][2];
    int iv[2];
    int i;
    unsigned long sz = (unsigned long)hdr->original_size;

    random_iv(iv);
    copy_block(header_blocks[0], iv);

    /* ---- Block 1: magic ------------------------------------------------ */
    memcpy(buf, HEADER_MAGIC, BLOCK_BYTES);
    pack_block(header_blocks[1], buf);

    /* ---- Block 2: original size --------------------------------------- */
    /* Pack original_size as 8 bytes in big-endian format */
    {
        size_t sz_copy = sz;
        int byte_idx;
        for (byte_idx = 7; byte_idx >= 0; byte_idx--)
        {
            buf[byte_idx] = (unsigned char)(sz_copy & 0xFF);
            sz_copy = sz_copy >> 8;
        }
    }
    pack_block(header_blocks[2], buf);

    /* ---- Blocks 3-10: filename ---------------------------------------- */
    {
        const char *bname = basename_only(hdr->filename);
        size_t bname_len = strlen(bname);
        int j;

        for (i = 0; i < HEADER_FNAME_BLOCKS; i++)
        {
            int offset = i * BLOCK_BYTES;
            for (j = 0; j < BLOCK_BYTES; j++)
            {
                size_t idx = (size_t)(offset + j);
                buf[j] = (idx < bname_len) ? (unsigned char)bname[idx] : 0;
            }
            pack_block(header_blocks[3 + i], buf);
        }
    }

    /* ---- Encrypt header metadata blocks in CBC order ------------------- */
    for (i = 1; i < HEADER_TOTAL_BLOCKS; i++)
    {
        xor_block(header_blocks[i], iv);
        encipher(rounds, header_blocks[i], key);
        copy_block(iv, header_blocks[i]);
    }

    /* ---- Prepend full header blocks in reverse order ------------------- */
    for (i = HEADER_TOTAL_BLOCKS - 1; i >= 0; i--)
    {
        BlockNode *node = (BlockNode *)malloc(sizeof(BlockNode));
        if (node == NULL)
        {
            fprintf(stderr, "ERROR: header_encode - malloc failed\n");
            return -1;
        }
        node->byte_block[0] = header_blocks[i][0];
        node->byte_block[1] = header_blocks[i][1];
        node->next_byte_block = *head;
        *head = node;
    }

#ifdef DEBUG
    fprintf(stderr, "[DEBUG] header_encode complete - %d blocks prepended\n",
            HEADER_TOTAL_BLOCKS);
    debug_print_header(hdr);
#endif

    return 0;
}

/*
 * header_decode
 * -------------
 * Pops and decrypts the first HEADER_TOTAL_BLOCKS nodes from *head.
 *
 * Return values:
 *   0   success - magic matched, hdr filled
 *  -1   wrong key or not an XTEA file - magic mismatch
 *  -2   structural error - list shorter than expected
 */
int header_decode(BlockNode **head, FileHeader *hdr, unsigned int rounds,
                  int const key[KEY_WORDS])
{
    unsigned char buf[BLOCK_BYTES];
    int i;
    int iv[2];
    int prev_cipher[2];
    int cipher_block[2];
    int plain_block[2];

    /* ---- Block 0: IV (plaintext) -------------------------------------- */
    if (pop_block(head, iv) != 0)
        return -2;
    copy_block(prev_cipher, iv);

    /* ---- Block 1: magic ----------------------------------------------- */
    if (pop_block(head, cipher_block) != 0)
        return -2;
    copy_block(plain_block, cipher_block);
    decipher(rounds, plain_block, key);
    xor_block(plain_block, iv);
    copy_block(prev_cipher, cipher_block);
    unpack_block(plain_block, buf);

    if (memcmp(buf, HEADER_MAGIC, BLOCK_BYTES) != 0)
    {
        fprintf(stderr, "ERROR: header magic mismatch - "
                        "wrong passphrase or not an XTEA-encrypted file\n");
        return -1;
    }

    /* ---- Block 2: original size --------------------------------------- */
    if (pop_block(head, cipher_block) != 0)
        return -2;
    copy_block(plain_block, cipher_block);
    decipher(rounds, plain_block, key);
    xor_block(plain_block, prev_cipher);
    copy_block(prev_cipher, cipher_block);
    unpack_block(plain_block, buf);
    {
        size_t decoded_size = 0;
        int byte_idx;
        for (byte_idx = 0; byte_idx < 8; byte_idx++)
        {
            decoded_size = (decoded_size << 8) | (unsigned long)buf[byte_idx];
        }
        hdr->original_size = decoded_size;
    }

    /* ---- Blocks 3-10: filename ---------------------------------------- */
    memset(hdr->filename, 0, HEADER_FNAME_BYTES);
    for (i = 0; i < HEADER_FNAME_BLOCKS; i++)
    {
        if (pop_block(head, cipher_block) != 0)
            return -2;
        copy_block(plain_block, cipher_block);
        decipher(rounds, plain_block, key);
        xor_block(plain_block, prev_cipher);
        copy_block(prev_cipher, cipher_block);
        unpack_block(plain_block, buf);
        memcpy(hdr->filename + i * BLOCK_BYTES, buf, BLOCK_BYTES);
    }
    /* guarantee null termination even if stored name filled all available bytes */
    hdr->filename[HEADER_FNAME_BYTES - 1] = '\0';

#ifdef DEBUG
    fprintf(stderr, "[DEBUG] header_decode complete - magic OK\n");
    debug_print_header(hdr);
#endif

    return 0;
}

int header_query(BlockNode **head, FileHeader *hdr, unsigned int rounds,
                  int const key[KEY_WORDS]) 
{
    unsigned char buf[BLOCK_BYTES];
    int i;
    int iv[2];
    int prev_cipher[2];
    int cipher_block[2];
    int plain_block[2];
    BlockNode *current_block = *head;

    /* ---- Block 0: IV (plaintext) -------------------------------------- */
    if (read_block(&current_block, iv) != 0)
        return -2;
    copy_block(prev_cipher, iv);
    current_block = current_block->next_byte_block;

    /* ---- Block 1: magic ----------------------------------------------- */
    if (read_block(&current_block, cipher_block) != 0)
        return -2;
    copy_block(plain_block, cipher_block);
    decipher(rounds, plain_block, key);
    xor_block(plain_block, iv);
    copy_block(prev_cipher, cipher_block);
    unpack_block(plain_block, buf);
    current_block = current_block->next_byte_block;

    if (memcmp(buf, HEADER_MAGIC, BLOCK_BYTES) != 0)
    {
        fprintf(stderr, "ERROR: header magic mismatch - "
                        "wrong passphrase or not an XTEA-encrypted file\n");
        return -1;
    }

    /* ---- Block 2: original size --------------------------------------- */
    if (read_block(&current_block, cipher_block) != 0)
        return -2;
    copy_block(plain_block, cipher_block);
    decipher(rounds, plain_block, key);
    xor_block(plain_block, prev_cipher);
    copy_block(prev_cipher, cipher_block);
    unpack_block(plain_block, buf);
    current_block = current_block->next_byte_block;
    {
        size_t decoded_size = 0;
        int byte_idx;
        for (byte_idx = 0; byte_idx < 8; byte_idx++)
        {
            decoded_size = (decoded_size << 8) | (unsigned long)buf[byte_idx];
        }
        hdr->original_size = decoded_size;
    }

    /* ---- Blocks 3-10: filename ---------------------------------------- */
    memset(hdr->filename, 0, HEADER_FNAME_BYTES);
    for (i = 0; i < HEADER_FNAME_BLOCKS; i++)
    {
        if (read_block(&current_block, cipher_block) != 0)
            return -2;
        copy_block(plain_block, cipher_block);
        decipher(rounds, plain_block, key);
        xor_block(plain_block, prev_cipher);
        copy_block(prev_cipher, cipher_block);
        unpack_block(plain_block, buf);
        memcpy(hdr->filename + i * BLOCK_BYTES, buf, BLOCK_BYTES);
        current_block = current_block->next_byte_block;
    }
    /* guarantee null termination even if stored name filled all available bytes */
    hdr->filename[HEADER_FNAME_BYTES - 1] = '\0';
    
    return 0;
}

/* =========================================================================
 * DEBUG HELPERS
 * ========================================================================= */

#ifdef DEBUG

void debug_print_block(const char *label, int byte_block_lower,
                       int byte_block_upper)
{
    fprintf(
        stderr, "[DEBUG] %-12s  byte_block[0]=0x%08X  byte_block[1]=0x%08X\n",
        label, (unsigned int)byte_block_lower, (unsigned int)byte_block_upper);
}

void debug_print_key(int const key[KEY_WORDS])
{
    int i;
    fprintf(stderr, "[DEBUG] KEY: ");
    for (i = 0; i < KEY_WORDS; i++)
    {
        fprintf(stderr, "0x%08X ", (unsigned int)key[i]);
    }
    fprintf(stderr, "\n");
}

void debug_print_list(const BlockNode *head)
{
    const BlockNode *current_byte_block = head;
    int idx = 0;
    fprintf(stderr, "[DEBUG] --- Block List ---\n");
    while (current_byte_block != NULL)
    {
        fprintf(stderr,
                "[DEBUG]  [%3d]  byte_block[0]=0x%08X  byte_block[1]=0x%08X\n",
                idx, (unsigned int)current_byte_block->byte_block[0],
                (unsigned int)current_byte_block->byte_block[1]);
        idx++;
        current_byte_block = current_byte_block->next_byte_block;
    }
    fprintf(stderr, "[DEBUG] --- End of List (%d blocks) ---\n", idx);
}

void debug_print_header(const FileHeader *hdr)
{
    fprintf(stderr, "[DEBUG] Header contents:\n");
    fprintf(stderr, "[DEBUG]   original_size = %lu bytes\n",
            (unsigned long)hdr->original_size);
    fprintf(stderr, "[DEBUG]   filename      = '%s'\n", hdr->filename);
}

#endif /* DEBUG */
