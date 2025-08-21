#include "dsd.h"

void ken_dmr_scrambler_keystream_creation(dsd_state * state, char * input)
{
  /*
  SLOT 1 Protected LC  FLCO=0x00 FID=0x20 <--this link appears to indicate scrambler usage from Kenwood on DMR
  DMR PDU Payload [80][20][40][00][00][01][00][00][01] SB: 00000000000 - 000;

  SLOT 1 TGT=1 SRC=1 FLCO=0x00 FID=0x00 SVC=0x00 Group Call <--different call, no scrambler from same Kenwood Radio
  DMR PDU Payload [00][00][00][00][00][01][00][00][01]

  For This, we could possible transition this to not be enforced
  since we may have a positive indicator in link control, 
  but needs further samples and validation
  */

  int lfsr = 0, bit = 0;
  sscanf (input, "%d", &lfsr);
  fprintf (stderr,"DMR Kenwood 15-bit Scrambler Key %05d with Forced Application\n", lfsr);

  for (int i = 0; i < 882; i++)
  {
    state->static_ks_bits[0][i] = lfsr & 0x1;
    state->static_ks_bits[1][i] = lfsr & 0x1;
    bit = ( (lfsr >> 1) ^ (lfsr >> 0) ) & 1;
    lfsr =  ( (lfsr >> 1 ) | (bit << 14) );
  }

  state->ken_sc = 1;

}

void anytone_bp_keystream_creation(dsd_state * state, char * input)
{
  uint16_t key = 0;
  uint16_t kperm = 0;
  
  sscanf (input, "%hX", &key);
  key &= 0xFFFF; //truncate to 16-bits

  //calculate key permutation using simple operations
  uint8_t nib1, nib2, nib3, nib4;

  //nib 1 and 3 are simple inversions
  nib1 = ~(key >> 12) & 0xF;
  nib3 = ~(key >> 4)  & 0xF;

  //nib 2 and 4 are +8 and mod 16 (& 0xF)
  nib2 = (((key >> 8) & 0xF) + 8) % 16;
  nib4 = (((key >> 0) & 0xF) + 8) % 16;

  //debug
  // fprintf (stderr, "{%01X, %01X, %01X, %01X}", nib1, nib2, nib3, nib4);

  kperm = nib1;
  kperm <<= 4;
  kperm |= nib2;
  kperm <<= 4;
  kperm |= nib3;
  kperm <<= 4;
  kperm |= nib4;

  //load bits into static keystream
  for (int i = 0; i < 16; i++)
  {
    state->static_ks_bits[0][i] = (kperm >> (15-i)) & 1;
    state->static_ks_bits[1][i] = (kperm >> (15-i)) & 1;
  }

  fprintf (stderr,"DMR Anytone Basic 16-bit Key 0x%04X with Forced Application\n", key);
  state->any_bp = 1;

}