#ifndef HELPERS_H
#define HELPERS_H

#include <stdio.h> /* FILE, size_t */

/* =========================================================================
 * CONSTANTS
 * ========================================================================= */

#define BLOCK_BYTES 8  /* XTEA operates on 8-byte (64-bit) blocks     */
#define KEY_WORDS   4  /* 128-bit key = four 32-bit words              */
#define KEY_BYTES   16 /* KEY_WORDS * 4                                */

/*
 * Header layout — the header is stored as an IV block followed by 10 CBC-
 * encrypted metadata blocks.
 *
 *  Block  0      iv         8 bytes   random CBC IV for header encryption
 *  Block  1      magic      8 bytes   literal "XTEAFILE"
 *  Block  2      size       8 bytes   original file size (upper/lower 32-bit)
 *  Blocks 3-10   filename   64 bytes  original filename, null-padded
 *
 *  Total: 11 blocks × 8 bytes = 88 bytes
 *
 *  The IV is stored in plaintext.  The remaining 10 blocks are encrypted
 *  in CBC mode using that IV.  On decryption the magic block is checked
 *  after CBC deciphering.
 */
#define HEADER_MAGIC        "XTEAFILE" /* exactly 8 chars, no null needed  */
#define HEADER_FNAME_BYTES  64         /* max stored filename length        */
#define HEADER_FNAME_BLOCKS 8          /* 64 / BLOCK_BYTES                  */
#define HEADER_TOTAL_BLOCKS 11         /* iv + magic + size + fname */
#define HEADER_TOTAL_BYTES  88         /* HEADER_TOTAL_BLOCKS * BLOCK_BYTES */

/* =========================================================================
 * FILE METADATA  —  populated by header_encode, decoded by header_decode
 * ========================================================================= */

typedef struct
{
    size_t original_size;              /* exact byte count of plaintext*/
    char filename[HEADER_FNAME_BYTES]; /* original filename, null-term*/
} FileHeader;

/* =========================================================================
 * BLOCK NODE  —  singly-linked list of 8-byte cipher blocks
 *
 *  Each node holds one XTEA block (two 32-bit integers = 8 bytes of data).
 *  The list is built while reading the file so we never need to know the
 *  file size up front — we just keep appending nodes.
 * ========================================================================= */

typedef struct BlockNode
{
    int byte_block[2];      /* one 64-bit XTEA block (byte_block[0] (left), 
                             * byte_block[1]) (right) */
    struct BlockNode *next_byte_block; /* pointer to next block in chain     */
} BlockNode;

/* =========================================================================
 * LINKED-LIST OPERATIONS
 * ========================================================================= */

/* Allocate and append a new node; returns pointer to it, NULL on failure */
BlockNode *list_append(BlockNode **head, BlockNode **tail, int byte_block_lower, int byte_block_upper);

/* Free every node in the list */
void list_free(BlockNode *head);

/* Count nodes in list */
size_t list_length(const BlockNode *head);

/* =========================================================================
 * CBC MODE HELPERS
 * ========================================================================= */

/* Generate a random Initialisation Vector for CBC */
void random_iv(int iv[2]);

/* Pop and remove the first block from the list */
int pop_block(BlockNode **head, int cipher_block[2]);

/* =========================================================================
 * ENCRYPT / DECRYPT  an entire linked list  (in-place)
 * ========================================================================= */

/* Pack 8 bytes (big-endian) into an int[2] block */
void pack_block(int byte_block[2], const unsigned char bytes[BLOCK_BYTES]);

/* Unpack an int[2] block into 8 bytes (big-endian) */
void unpack_block(const int byte_block[2], unsigned char bytes[BLOCK_BYTES]);

/* =========================================================================
 * KEY HELPERS
 * ========================================================================= */

/* Pack 16 raw bytes into a key[4] array (big-endian) */
void pack_key(const unsigned char raw[KEY_BYTES], int key[KEY_WORDS]);

/*
 * Derive a key[4] from a plain-text passphrase using a djb2-style hash
 * spread across the four key words with distinct prime seeds.
 */
void key_from_passphrase(const char *passphrase, int key[KEY_WORDS]);

/* =========================================================================
 * FILE  ->  LINKED LIST  (read & pad)
 *
 *  Reads a binary file into a linked list of BlockNodes.
 *  The last block is zero-padded if needed.
 *  *original_size is set to the exact pre-padding byte count.
 *
 *  Returns head of list, or NULL on error.
 * ========================================================================= */
BlockNode *file_to_list(const char *filename, size_t *original_size);

/* =========================================================================
 * LINKED LIST  ->  FILE  (write & strip)
 *
 *  Writes the list to a file, truncating to exactly original_size bytes
 *  to strip zero-padding from the final block.
 *
 *  Returns 0 on success, -1 on error.
 * ========================================================================= */
int list_to_file(const char *filename, const BlockNode *head,
                 size_t original_size);

/* =========================================================================
 * ENCRYPT / DECRYPT  an entire linked list  (in-place)
 * ========================================================================= */

void list_encipher(BlockNode *head, unsigned int rounds,
                   int const key[KEY_WORDS], int iv[2]);

void list_decipher(BlockNode *head, unsigned int rounds,
                   int const key[KEY_WORDS], int iv[2]);

/* =========================================================================
 * HEADER ENCODE / DECODE
 *
 *  header_encode — builds the 11-block header from a FileHeader struct,
 *                  encrypts 10 metadata blocks in CBC mode, and prepends
 *                  the IV + encrypted blocks to *head.
 *
 *  header_decode — pops and deciphers the first 11 blocks of *head, checks
 *                  the magic phrase, and fills a FileHeader struct.
 *                  Returns  0 if the magic matches   (key correct).
 *                  Returns -1 if the magic is wrong  (key incorrect / not
 *                  an XTEA-encrypted file) — caller should abort.
 *                  Returns -2 on structural error    (list too short, etc.)
 * ========================================================================= */

int header_encode(BlockNode **head, const FileHeader *hdr, unsigned int rounds,
                  int const key[KEY_WORDS]);

int header_decode(BlockNode **head, FileHeader *hdr, unsigned int rounds,
                  int const key[KEY_WORDS]);

/* =========================================================================
 * FILENAME UTILITIES
 * ========================================================================= */

/*
 * build_output_name
 * -----------------
 * If -o was not supplied, derive the output filename automatically:
 *   encrypt:  append ".enc"           e.g. photo.jpg  -> photo.jpg.enc
 *   decrypt:  strip trailing ".enc"   e.g. photo.jpg.enc -> photo.jpg
 *             if no ".enc" suffix, append ".dec" to avoid overwrite
 *
 * Writes result into out_buf (caller supplies buffer of size out_buf_len).
 * Returns 0 on success, -1 if the result would overflow the buffer.
 */
int build_output_name(const char *infile, int encrypt_mode, char *out_buf,
                      size_t out_buf_len);

/*
 * files_are_same
 * --------------
 * Returns 1 if infile and outfile resolve to the same path, 0 otherwise.
 * Simple string comparison — sufficient when both names come from argv.
 */
int files_are_same(const char *a, const char *b);

/*
 * extract_name
 * -------------
 * Extracts the filename from a full path, excluding the directory and 
 * extension.
 */
void extract_name(const char *path, char *out, size_t out_size);

/* =========================================================================
 * DEBUG HELPERS  (compiled away when DEBUG is not defined)
 * ========================================================================= */

#ifdef DEBUG
void debug_print_block(const char *label, int v0, int v1);
void debug_print_key(int const key[KEY_WORDS]);
void debug_print_list(const BlockNode *head);
void debug_print_header(const FileHeader *hdr);
#endif

#endif /* HELPERS_H */
