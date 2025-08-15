#include "dsd.h"

//de-interleaved code words for AMBE+2
void ambe2_codeword_print_b (dsd_opts * opts, char ambe_fr[4][24])
{
  uint8_t fr_reverse[4][24]; memset(fr_reverse, 0, sizeof(fr_reverse));
  for (int i = 0; i < 24; i++)
    fr_reverse[0][i] = ambe_fr[0][23-i];
  for (int i = 0; i < 23; i++)
    fr_reverse[1][i] = ambe_fr[1][22-i];
  for (int i = 0; i < 11; i++)
    fr_reverse[2][i] = ambe_fr[2][10-i];
  for (int i = 0; i < 14; i++)
    fr_reverse[3][i] = ambe_fr[3][13-i];

  uint32_t v0 = (uint32_t)convert_bits_into_output((uint8_t *)fr_reverse[0], 24); //24
  uint32_t v1 = (uint32_t)convert_bits_into_output((uint8_t *)fr_reverse[1], 23); //23
  uint32_t v2 = (uint32_t)convert_bits_into_output((uint8_t *)fr_reverse[2], 11); //11
  uint32_t v3 = (uint32_t)convert_bits_into_output((uint8_t *)fr_reverse[3], 14); //14
  
  uint32_t c0 = (uint32_t)convert_bits_into_output((uint8_t *)fr_reverse[0], 12);
  uint32_t c1 = (uint32_t)convert_bits_into_output((uint8_t *)fr_reverse[1], 12);

  //72 bit version
  unsigned long long int hex1 = ((unsigned long long int)v0 << 40ULL) + ((unsigned long long int)v1 << 17ULL) + ((unsigned long long int)v2 << 6ULL) + (v3 >> 8); 
  unsigned long long int hex2 = v3 & 0xFF;

  //49 bit version prior to golay correction and c1 demodulation pN
  unsigned long long int hex49 = ((unsigned long long int)c0 << 37ULL) + ((unsigned long long int)c1 << 25ULL) + ((unsigned long long int)v2 << 14ULL) + v3;

  UNUSED(opts);

  if (opts->payload == 1)
  {
    fprintf (stderr, " AMBE HEX(72): %016llX%02llX \n", hex1, hex2);
    fprintf (stderr, " AMBE HEX(49): %014llX\n", hex49 << 7);
  }

}

//de-interleaved code words for AMBE+2
void ambe2_codeword_print_f (dsd_opts * opts, char ambe_fr[4][24])
{
  uint32_t v0 = (uint32_t)convert_bits_into_output((uint8_t *)ambe_fr[0], 24); //24
  uint32_t v1 = (uint32_t)convert_bits_into_output((uint8_t *)ambe_fr[1], 23); //23
  uint32_t v2 = (uint32_t)convert_bits_into_output((uint8_t *)ambe_fr[2], 11); //11
  uint32_t v3 = (uint32_t)convert_bits_into_output((uint8_t *)ambe_fr[3], 14); //14

  // if (opts->payload == 1)
  //   fprintf (stderr, " AMBE V0: %06X; V1: %06X; V2: %03X; V3: %04X; \n", v0, v1, v2, v3);

  unsigned long long int hex1 = ((unsigned long long int)v0 << 40ULL) + ((unsigned long long int)v1 << 17ULL) + ((unsigned long long int)v2 << 6ULL) + (v3 >> 8); 
  unsigned long long int hex2 = v3 & 0xFF;

  if (opts->payload == 1)
    fprintf (stderr, " AMBE HEX(72): %016llX%02llX \n", hex1, hex2);

}

//test application of keystream to codewords instead of ambe_d (tytera / retevis, etc)
int tyt16_ambe2_codeword_keystream(dsd_state * state, char ambe_fr[4][24], int idx, int fnum)
{
  uint8_t ks_bytes[28]; memset(ks_bytes, 0, sizeof(ks_bytes));
  uint8_t ks[224]; memset(ks, 0, sizeof(ks));

  UNUSED(fnum);

  ks_bytes[0] = (state->H >> 8) & 0xFF;
  ks_bytes[1] = (state->H >> 0) & 0xFF;

  //copy same bytes into rest of byte array
  for (int16_t i = 2; i < 28; i++)
    ks_bytes[i] = ks_bytes[i%2];

  //debug ks_bytes
  // fprintf (stderr, " KB: ");
  // for (int16_t i = 0; i < 28; i++)
  //   fprintf (stderr, "%02X ", ks_bytes[i]);
  // fprintf (stderr, "\n");

  //convert byte array into a bit array
  unpack_byte_array_into_bit_array(ks_bytes, ks, 28);

  //debug ks
  // fprintf (stderr, " KS: ");
  // for (int16_t i = 0; i < 28; i++)
  //   fprintf (stderr, "%02X ", (uint8_t)convert_bits_into_output(ks+(i*8), 8));
  // fprintf (stderr, "\n");

  //debug to test application to ambe_fr result
  // memset(ambe_fr, 0, 4*24*sizeof(char));

  if (fnum == 0)
    idx = 0;
  else idx = 8;

  //straight?
  for (int16_t i = 0; i < 24; i++)
    ambe_fr[0][i] ^= ks[((idx++)%216)] ^ 1; //%216

  for (int16_t i = 0; i < 23; i++)
    ambe_fr[1][i] ^= ks[((idx++)%216)] ^ 1; //%216

  for (int16_t i = 0; i < 11; i++)
    ambe_fr[2][i] ^= ks[((idx++)%216)] ^ 1; //%216

  for (int16_t i = 0; i < 14; i++)
    ambe_fr[3][i] ^= ks[((idx++)%216)] ^ 1; //%216

  //backwards?
  // for (int16_t i = 13; i >= 0; i--)
  //   ambe_fr[3][i] ^= ks[((idx++)%216)] ^ 1; //%216

  // for (int16_t i = 10; i >= 0; i--)
  //   ambe_fr[2][i] ^= ks[((idx++)%216)] ^ 1; //%216

  // for (int16_t i = 22; i >= 0; i--)
  //   ambe_fr[1][i] ^= ks[((idx++)%216)] ^ 1; //%216

  // for (int16_t i = 23; i >= 0; i--)
  //   ambe_fr[0][i] ^= ks[((idx++)%216)] ^ 1; //%216

  //forwards?
  // for (int16_t i = 23; i >= 0; i--)
  //   ambe_fr[0][i] ^= ks[((idx++)%216)] ^ 1; //%216

  // for (int16_t i = 22; i >= 0; i--)
  //   ambe_fr[1][i] ^= ks[((idx++)%216)] ^ 1; //%216

  // for (int16_t i = 10; i >= 0; i--)
  //   ambe_fr[2][i] ^= ks[((idx++)%216)] ^ 1; //%216

  // for (int16_t i = 13; i >= 0; i--)
  //   ambe_fr[3][i] ^= ks[((idx++)%216)] ^ 1; //%216

  //debug idx value
  fprintf (stderr, " KS IDX: %04d; \n", idx);

  return idx;


}