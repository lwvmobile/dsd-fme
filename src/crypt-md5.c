//MD5 Code Originally Sourced From: https://github.com/Zunawe/md5-c

/*
 * Derived from the RSA Data Security, Inc. MD5 Message-Digest Algorithm
 * and modified slightly to be functionally identical but condensed into control structures.
 */

#include "dsd.h"

typedef struct{
    uint64_t size;        // Size of input in bytes
    uint32_t buffer[4];   // Current accumulation of hash
    uint8_t input[64];    // Input to be used in the next step
    uint8_t digest[16];   // Result of algorithm
}MD5Context;

/*
 * Constants defined by the MD5 algorithm
 */
#define A 0x67452301
#define B 0xefcdab89
#define C 0x98badcfe
#define D 0x10325476

static uint32_t S[] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                       5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
                       4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                       6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

static uint32_t K[] = {0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
                       0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                       0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
                       0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                       0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
                       0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                       0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
                       0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                       0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
                       0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                       0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
                       0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                       0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
                       0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                       0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
                       0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

/*
 * Padding used to make the size (in bits) of the input congruent to 448 mod 512
 */
static uint8_t PADDING[] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/*
 * Bit-manipulation functions defined by the MD5 algorithm
 */
#define F(X, Y, Z) ((X & Y) | (~X & Z))
#define G(X, Y, Z) ((X & Z) | (Y & ~Z))
#define H(X, Y, Z) (X ^ Y ^ Z)
#define I(X, Y, Z) (Y ^ (X | ~Z))

/*
 * Rotates a 32-bit word left by n bits
 */
uint32_t rotateLeft(uint32_t x, uint32_t n){
    return (x << n) | (x >> (32 - n));
}


/*
 * Initialize a context
 */
void md5Init(MD5Context *ctx){
    ctx->size = (uint64_t)0;

    ctx->buffer[0] = (uint32_t)A;
    ctx->buffer[1] = (uint32_t)B;
    ctx->buffer[2] = (uint32_t)C;
    ctx->buffer[3] = (uint32_t)D;
}

/*
 * Step on 512 bits of input with the main MD5 algorithm.
 */
void md5Step(uint32_t *buffer, uint32_t *input){
    uint32_t AA = buffer[0];
    uint32_t BB = buffer[1];
    uint32_t CC = buffer[2];
    uint32_t DD = buffer[3];

    uint32_t E;

    unsigned int j;

    for(unsigned int i = 0; i < 64; ++i){
        switch(i / 16){
            case 0:
                E = F(BB, CC, DD);
                j = i;
                break;
            case 1:
                E = G(BB, CC, DD);
                j = ((i * 5) + 1) % 16;
                break;
            case 2:
                E = H(BB, CC, DD);
                j = ((i * 3) + 5) % 16;
                break;
            default:
                E = I(BB, CC, DD);
                j = (i * 7) % 16;
                break;
        }

        uint32_t temp = DD;
        DD = CC;
        CC = BB;
        BB = BB + rotateLeft(AA + E + K[i] + input[j], S[i]);
        AA = temp;
    }

    buffer[0] += AA;
    buffer[1] += BB;
    buffer[2] += CC;
    buffer[3] += DD;
}

/*
 * Add some amount of input to the context
 *
 * If the input fills out a block of 512 bits, apply the algorithm (md5Step)
 * and save the result in the buffer. Also updates the overall size.
 */
void md5Update(MD5Context *ctx, uint8_t *input_buffer, size_t input_len){
    uint32_t input[16];
    unsigned int offset = ctx->size % 64;
    ctx->size += (uint64_t)input_len;

    // Copy each byte in input_buffer into the next space in our context input
    for(unsigned int i = 0; i < input_len; ++i){
        ctx->input[offset++] = (uint8_t)*(input_buffer + i);

        // If we've filled our context input, copy it into our local array input
        // then reset the offset to 0 and fill in a new buffer.
        // Every time we fill out a chunk, we run it through the algorithm
        // to enable some back and forth between cpu and i/o
        if(offset % 64 == 0){
            for(unsigned int j = 0; j < 16; ++j){
                // Convert to little-endian
                // The local variable `input` our 512-bit chunk separated into 32-bit words
                // we can use in calculations
                input[j] = (uint32_t)(ctx->input[(j * 4) + 3]) << 24 |
                           (uint32_t)(ctx->input[(j * 4) + 2]) << 16 |
                           (uint32_t)(ctx->input[(j * 4) + 1]) <<  8 |
                           (uint32_t)(ctx->input[(j * 4)]);
            }
            md5Step(ctx->buffer, input);
            offset = 0;
        }
    }
}

/*
 * Pad the current input to get to 448 bytes, append the size in bits to the very end,
 * and save the result of the final iteration into digest.
 */
void md5Finalize(MD5Context *ctx){
    uint32_t input[16];
    unsigned int offset = ctx->size % 64;
    unsigned int padding_length = offset < 56 ? 56 - offset : (56 + 64) - offset;

    // Fill in the padding and undo the changes to size that resulted from the update
    md5Update(ctx, PADDING, padding_length);
    ctx->size -= (uint64_t)padding_length;

    // Do a final update (internal to this function)
    // Last two 32-bit words are the two halves of the size (converted from bytes to bits)
    for(unsigned int j = 0; j < 14; ++j){
        input[j] = (uint32_t)(ctx->input[(j * 4) + 3]) << 24 |
                   (uint32_t)(ctx->input[(j * 4) + 2]) << 16 |
                   (uint32_t)(ctx->input[(j * 4) + 1]) <<  8 |
                   (uint32_t)(ctx->input[(j * 4)]);
    }
    input[14] = (uint32_t)(ctx->size * 8);
    input[15] = (uint32_t)((ctx->size * 8) >> 32);

    md5Step(ctx->buffer, input);

    // Move the result into digest (convert from little-endian)
    for(unsigned int i = 0; i < 4; ++i){
        ctx->digest[(i * 4) + 0] = (uint8_t)((ctx->buffer[i] & 0x000000FF));
        ctx->digest[(i * 4) + 1] = (uint8_t)((ctx->buffer[i] & 0x0000FF00) >>  8);
        ctx->digest[(i * 4) + 2] = (uint8_t)((ctx->buffer[i] & 0x00FF0000) >> 16);
        ctx->digest[(i * 4) + 3] = (uint8_t)((ctx->buffer[i] & 0xFF000000) >> 24);
    }
}

/*
 * Functions that run the algorithm on the provided input and put the digest into result.
 * result should be able to store 16 bytes.
 */
void md5String(char *input, uint8_t *result){
    MD5Context ctx;
    md5Init(&ctx);
    md5Update(&ctx, (uint8_t *)input, strlen(input));
    md5Finalize(&ctx);

    memcpy(result, ctx.digest, 16);
}

void md5File(FILE *file, uint8_t *result){
    char *input_buffer = malloc(1024);
    size_t input_size = 0;

    MD5Context ctx;
    md5Init(&ctx);

    while((input_size = fread(input_buffer, 1, 1024, file)) > 0){
        md5Update(&ctx, (uint8_t *)input_buffer, input_size);
    }

    md5Finalize(&ctx);

    free(input_buffer);

    memcpy(result, ctx.digest, 16);
}

/**
 * RAS 104-bit permutation
 *
 * Generates a permutation of bit positions 0..103 using the 32-bit seed
 * (the last four active key bytes interpreted as a big-endian uint32).
 *
 * Algorithm:
 *   - Maintain a circular list of the still-available positions.
 *   - Repeatedly select (seed % remaining_count) steps from the current
 *     position, record that index, remove it, and continue from its successor.
 *
 * Parameters:
 *   seed  - 32-bit seed (e.g. 0x92604BD5)
 *   order - output array of 104 integers. order[i] = source bit position
 *           that should be placed at output bit i.
 */
void ras_permute_order(uint32_t seed, int * order)
{
  int remaining[104];
  int count = 104;
  int pos   = 0;          /* current index inside the remaining[] array */

  for (int i = 0; i < 104; i++)
    remaining[i] = i;

  for (int i = 0; i < 104; i++)
  {
    unsigned idx  = seed % (unsigned)count;
    int      actual = (pos + (int)idx) % count;

    order[i] = remaining[actual];

    #ifdef DEBUG_RAS
    //debug
    // fprintf (stderr, "\n I: %d; C: %d; P: %d; IDX: %u; ACT: %d; ORD: %d; ", i, count, pos, idx, actual, order[i]);
    #endif

    /* remove the chosen entry */
    for (int j = actual; j < count - 1; j++)
      remaining[j] = remaining[j + 1];
    count--;

    /* continue from the successor of the removed element */
    if (count > 0)
      pos = actual % count;
  }

  #ifdef DEBUG_RAS
  //debug
  // fprintf(stderr, "\n Order: ");
  // for (int i = 0; i < 104; i++)
  //   fprintf (stderr, "%03d,", order[i]);
  #endif
}

void apply_ras_permutation(const uint8_t * src, uint8_t * dst, const int * order)
{
  for (int i = 0; i < 104; i++)
  {
    int src_bit  = order[i];
    int src_byte = src_bit / 8;
    int src_off  = 7 - (src_bit % 8);   /* MSB of byte is lowest bit index */

    int bit = (src[src_byte] >> src_off) & 1;

    int dst_byte = i / 8;
    int dst_off  = 7 - (i % 8);

    if (bit)
      dst[dst_byte] |= (uint8_t)(1u << dst_off);
  }
}

//Full RAS workflow, return value is the the calculated MAC attachment
//input is a pointer to byte-wise payload from dburst of RAS enabled payloads
uint32_t ras_mac_calculator(dsd_state * state, uint8_t * input, int input_len, int type)
{

  if (state->ras_effective_key == 0)
    return 0;

  uint32_t mac = 0;
  uint8_t ras_input[13];
  memset (ras_input, 0, sizeof(ras_input));

  for (int i = 0; i < input_len; i++)
    ras_input[i] = input[i];

  int shift = 48;
  for (int i = input_len; i < 13; i++)
  {
    ras_input[i] = (state->ras_effective_key >> shift) & 0xFF;
    shift -= 8;
  }

  #ifdef DEBUG_RAS
  //debug input value
  if (type != 2)
  {
    fprintf (stderr, "\n RAS  Input: ");
    for (int i = 0; i < 13; i++)
      fprintf (stderr, "%02X", ras_input[i]);
  }
  #endif

  uint8_t ras_output[16]; memset(ras_output, 0, sizeof(ras_output));

  if (type != 2)
  {
    apply_ras_permutation(ras_input, ras_output, state->ras_permutation_order);

    #ifdef DEBUG_RAS
    //debug output value
    fprintf (stderr, "\n RAS Output: ");
    for (int i = 0; i < 13; i++)
      fprintf (stderr, "%02X", ras_output[i]);
    #endif
  }

  //Run Appropriate CRC or RS calculation on Permuted Output
  uint8_t output_bits[104]; memset(output_bits, 0, sizeof(output_bits));
  unpack_byte_array_into_bit_array(ras_output, output_bits, 13);

  if (type == 0)
    mac = ComputeCrcCCITT16d(output_bits, 13*8);
  else if (type == 1)
  {
    rs_16_13_encode(ras_output);
    for (int i = 0; i < 3; i++)
    {
      mac <<= 8;
      mac |= ras_output[i+13];
    }
  }
  else if (type == 2)
  {
    mac  = ComputeCrc5Bit(input);
    mac += state->ras_effective_key & 0x1F;
    mac %= 31;
  }

  return mac;

}

void ras_effective_key_creation(dsd_state * state, char * input)
{

  uint8_t md5_key[30];
  memset (md5_key, 0, sizeof(md5_key));

  uint16_t len = strlen((const char*)input);

  //sanity check on provided char len
  if (len > 24)
    len = 24;

  //load chars as uint8_t into key array
  for (int i = 0; i < len; i++)
    md5_key[i] = (uint8_t)input[i];

  //debug
  fprintf (stderr, "RAS Key String: %s; LEN: %d; HEX: ", input, len);
  for (int i = 0; i < len; i++)
    fprintf (stderr, "%02X ", md5_key[i]);
  fprintf (stderr, "\n");

  MD5Context ctx;
  md5Init(&ctx);
  md5Update(&ctx, md5_key, len);
  md5Finalize(&ctx);

  //debug dump the digest
  fprintf (stderr, "MD5: ");
  for (int i = 0; i < 16; i++)
    fprintf (stderr, "%02X ", ctx.digest[i]);
  fprintf (stderr, "\n");

  //collect 7 bytes of the MD5 Hash to create the flat key value
  unsigned long long int flat_key = 0;
  for (int i = 0; i < 7; i++)
  {
    flat_key <<= 8;
    flat_key |= ctx.digest[i];
  }

  //On real world testing, it was discovered that by selecting a password (VluByDZhkPenguinkeeper) which has
  //an MD5 partial hash of FFFFFFFFFEDFFF, the value actually placed into the radio was FFFFFFFFFEE001
  //this indicates the entire value will roll-over, and not just a byte rollvoer (mod)

  //The known rule is that the last 5 bits of the flat key value (prior to reverse)
  //is that it cannot equal 0x1F (all 1 bits), if so, a correction of +2 is 
  //performaed onto a flat value, which increments the entire value appropriately

  //Below patent also suggest all bytes cannot equal zero
  //and if so, the next portion of MD5 may be used,
  //or some other method may be utilized to prevent fringe cases
  //https://patents.google.com/patent/US20130288643A1

  if ((flat_key & 0x1F) == 0x1F)
  {
    fprintf (stderr, "Flat Correction: %014llX -> %014llX;\n", flat_key, flat_key+2);
    flat_key += 2;
  }

  //byte-reverse the flat_key into the effective_key
  unsigned long long int effective_key = 0;
  for (int i = 0; i < 7; i++)
  {
    effective_key <<= 8;
    effective_key |= (flat_key >> (i*8)) & 0xFF;
  }

  uint32_t ras_seed = effective_key & 0xFFFFFFFF;

  ras_permute_order(ras_seed, state->ras_permutation_order);

  //debug
  fprintf (stderr, "RAS Effective Key Value: %014llX;", effective_key);

  state->ras_effective_key = effective_key;

  fprintf (stderr, "\n");

}
