#include "encryption.h"

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
    int byte_block_lower = byte_block[0],
        byte_block_upper = byte_block[1], sum = 0, delta = DELTA;
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
    int byte_block_lower = byte_block[0],
        byte_block_upper = byte_block[1], delta = DELTA,
        sum = delta * iteration_count;
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