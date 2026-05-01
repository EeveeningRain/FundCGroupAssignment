#ifndef encryption
#define encryption

/* XTEA cipher - 64-bit block, 128-bit key */

void encipher(unsigned int iteration_count, int byte_block[2], int const secret_key[4]);
void decipher(unsigned int iteration_count, int byte_block[2], int const secret_key[4]);

#endif /* ENCRYPTION_H */