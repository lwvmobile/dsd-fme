#include "dsd.h"
#include "dmr_const.h"

//interleaved code words for AMBE+2
void ambe2_codeword_print_i (dsd_opts * opts, char ambe_fr[4][24])
{
  uint8_t interleaved[72];
  memset (interleaved, 0, sizeof(interleaved));

  //reinterleave the frame
  const int *w, *x, *y, *z;
  w = rW; x = rX; y = rY; z = rZ;

  for (int8_t i = 0; i < 36; i++)
  {
    interleaved[(i*2)+0] = (uint8_t)ambe_fr[*w][*x];
    interleaved[(i*2)+1] = (uint8_t)ambe_fr[*y][*z];

    w++;
    x++;
    y++;
    z++;
  }

  uint8_t bytes[9]; memset(bytes, 0, sizeof(bytes));

  //pack
  pack_bit_array_into_byte_array(interleaved, bytes, 9);

  if (opts->payload == 1)
  {
    fprintf (stderr, " AMBE HEX(72) INT: ");
    for (int8_t i = 0; i < 9; i++)
      fprintf (stderr, "%02X", bytes[i]);
    fprintf (stderr, "\n");
  }
    
}

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

//tested working perfectly fine on some Tytera BP samples, but not on others
//is there two or more different Tytera (or CCR) BP modes depending on FW?
void tyt16_ambe2_codeword_keystream(dsd_state * state, char ambe_fr[4][24], int fnum)
{

  char interleaved[72];
  memset (interleaved, 0, sizeof(interleaved));

  //interleave the frame
  const int *w, *x, *y, *z;
  w = rW; x = rX; y = rY; z = rZ;

  for (int8_t i = 0; i < 36; i++)
  {
    interleaved[(i*2)+0] = ambe_fr[*w][*x];
    interleaved[(i*2)+1] = ambe_fr[*y][*z];

    w++;
    x++;
    y++;
    z++;
  }

  uint8_t ks_bytes[10]; memset(ks_bytes, 0, sizeof(ks_bytes));
  uint8_t ks[80]; memset(ks, 0, sizeof(ks));

  ks_bytes[0] = (state->H >> 8) & 0xFF;
  ks_bytes[1] = (state->H >> 0) & 0xFF;

  //copy same bytes into rest of byte array
  for (int16_t i = 2; i < 10; i++)
    ks_bytes[i] = ks_bytes[i%2];

  //convert byte array into a bit array
  unpack_byte_array_into_bit_array(ks_bytes, ks, 10);

  //set ks idx position (-1)
  int idx = 0;
  if (fnum == 0)
    idx = 79;
  else idx = 71;

  //apply keystream to interleave
  for (int8_t i = 0; i < 72; i++)
    interleaved[i] ^= ks[idx--];

  //deinterleave back into ambe_fr frame
  w = rW; x = rX; y = rY; z = rZ;
  int k = 0;
  for (int8_t i = 0; i < 36; i++)
  {
    ambe_fr[*w][*x] = interleaved[k++];
    ambe_fr[*y][*z] = interleaved[k++];

    w++;
    x++;
    y++;
    z++;
  }

}