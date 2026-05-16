#ifndef encryption
#define encryption

/* XTEA cipher - 64-bit block, 128-bit key */

void encipher(unsigned int iteration_count, int byte_block[2],
              int const secret_key[4]);
void decipher(unsigned int iteration_count, int byte_block[2],
              int const secret_key[4]);

int do_encryption(const int mode, const char *infile, const char *outfile,
                  const char *passphrase, const unsigned int rounds);

#endif /* ENCRYPTION_H */