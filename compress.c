#include "securedisk.h"
#include "compress.h"

/*
LZW ENCODING
  *     PSEUDOCODE
  1     Initialize table with single character strings
  2     P = first input character
  3     WHILE not end of input stream
  4          C = next input character
  5          IF P + C is in the string table
  6            P = P + C
  7          ELSE
  8            output the code for P
  9          add P + C to the string table
  10           P = C
  11         END WHILE
  12    output code for P 

note concept of the LZ triple, noted that making a struct that can store a piece of compressed data may help

after that point, convert text in to lz triples

pattern match (any algorithm..)

convert triples to bits

*/




/* temp code from elsewhere just to have as a scaffold */



#pragma pack(push, 1) // there is a clang warn here, seems clangd bug
typedef struct Header {
  int szIn;
  char width;
} Header_t;
#pragma pack(pop)
_Static_assert(5 == sizeof(Header_t), "Wrong alignment");

#pragma pack(push, 1)
typedef struct Body {
  struct {
    char lo;
    char hi;
  } distlen; // distance and length to match, lower @width bits it len
  char symbol;
} Body_t;
#pragma pack(pop)
_Static_assert(3 == sizeof(Body_t), "Wrong alignment");

typedef struct EncodedData {
  Header_t header;
  Body_t body[0];
} EncodedData_t;

int encode(const char *in, int szIn, char *out, char wid) {
  EncodedData_t *outdat = (EncodedData_t *)out;

  const long maxDistMatch = 1 << ((8 * sizeof(long)) - wid);
  const long maxLenMatch = 1 << (wid);

  // put raw data size and @len width in the output header
  outdat->header.szIn = szIn;
  outdat->header.width = wid;

  // real compressed data begin
  int ofsSym, ofsOut = 0;
  for (int ofsIn = 0; ofsIn < szIn; ++ofsIn, ++ofsOut) {
    long distMatch = 0, lenMatch = 0, dist_len;

    // check search buffer
    for (long dm = 1, lm; (dm < maxDistMatch) && (dm <= ofsIn); ++dm) {
      int ofsInNew = ofsIn;
      int ofsMatch = ofsIn - dm;

      // check match length
      for (lm = 0; ofsInNew < szIn && in[ofsInNew++] == in[ofsMatch++]; ++lm) {
        if (lm == maxLenMatch)
          break;
      }

      // may have multiple repeat substring, choose better match
      if (lm > lenMatch) {
        distMatch = dm;
        lenMatch = lm;

        // best case found
        if (lenMatch == maxLenMatch)
          break;
      }
    }

    // construct dist|len, the lower @wid bits is len
    ofsIn += lenMatch;
    if ((ofsIn == szIn) && lenMatch) {
      dist_len = (lenMatch == 1) ? 0 : ((distMatch << wid) | (lenMatch - 2));
      ofsSym = ofsIn - 1;
    } else {
      dist_len = (distMatch << wid) | (lenMatch ? (lenMatch - 1) : 0);
      ofsSym = ofsIn;
    }

    // write data body
    outdat->body[ofsOut].distlen.lo = dist_len & 0xFF;
    outdat->body[ofsOut].distlen.hi = (dist_len >> 8) & 0xFF;
    outdat->body[ofsOut].symbol = *(in + ofsSym);
  }
  return sizeof(Header_t) + sizeof(Body_t) * ofsOut;
}

int decode(const char *in, char *out) {
  const EncodedData_t *indat = (EncodedData_t *)in;

  char wid = indat->header.width;
  int szOut = indat->header.szIn;
  long dist_len_mask = (1 << wid) - 1;

  // start decoding body
  int ofsOut = 0;
  for (int ofsIn = 0; ofsOut < szOut; ++ofsIn, ++ofsOut) {
    long lenMatch, distMatch, dist_len;

    *((char *)&dist_len) = indat->body[ofsIn].distlen.lo;
    *((char *)&dist_len + 1) = indat->body[ofsIn].distlen.hi;

    // copy repeated data from window
    distMatch = dist_len >> wid;
    lenMatch = distMatch ? ((dist_len & dist_len_mask) + 1) : 0;
    if (distMatch)
      for (int ofsMatch = ofsOut - distMatch; lenMatch > 0; --lenMatch)
        out[ofsOut++] = out[ofsMatch++];
    *(out + ofsOut) = indat->body[ofsIn].symbol;
  }

  return ofsOut;
}
