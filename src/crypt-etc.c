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