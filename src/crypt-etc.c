#include "dsd.h"
#include "dmr_const.h"

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

void straight_mod_xor_keystream_creation(dsd_state * state, char * input)
{
  uint16_t len = 0;
  char * curr;
  curr = strtok(input, ":"); //should be len (mod) of key (decimal)
  if (curr != NULL)
    sscanf (curr, "%hd", &len);
  else goto END_KS;

  //len sanity check, can't be greater than 882
  if (len > 882)
    len = 882;

  curr = strtok(NULL, ":"); //should be key in hex
  if (curr != NULL)
  {
    //continue
  }
  else goto END_KS;

  uint8_t ks_bytes[112];
  memset (ks_bytes, 0, sizeof(ks_bytes));
  parse_raw_user_string(curr, ks_bytes);

  uint8_t ks_bits[896];
  memset(ks_bits, 0, sizeof(ks_bits));

  uint16_t unpack_len = len / 8;
  if (len % 8)
    unpack_len++;
  unpack_byte_array_into_bit_array(ks_bytes, ks_bits, unpack_len);

  for (uint16_t i = 0; i < len; i++)
  {
    state->static_ks_bits[0][i] = ks_bits[i];
    state->static_ks_bits[1][i] = ks_bits[i];
  }

  fprintf (stderr,"AMBE Straight XOR %d-bit Keystream: ", len);
  for (uint16_t i = 0; i < unpack_len; i++)
    fprintf (stderr, "%02X", ks_bytes[i]);
  fprintf (stderr, " with Forced Application \n");

  state->straight_ks = 1;
  state->straight_mod = (int)len;

  END_KS:

  if (curr == NULL)
    fprintf (stderr, "Straight KS String Malformed! No KS Created!\n");

}

//connect systems 72-bit (9-byte) Extended Encryption //TODO: Move this later
void csi72_ambe2_codeword_keystream(dsd_state * state, char ambe_fr[4][24])
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

  uint8_t ks_bytes[9]; memset(ks_bytes, 0, sizeof(ks_bytes));
  uint8_t ks[72]; memset(ks, 0, sizeof(ks));

  //keys are loaded in reverse byte order, stored in state->static_ks_bits[0] 
  for (int i = 0; i < 9; i++)
    ks_bytes[i] = state->static_ks_bits[0][8-i];

  //convert byte array into a bit array
  unpack_byte_array_into_bit_array(ks_bytes, ks, 9);

  //apply keystream to interleave
  for (int8_t i = 0; i < 72; i++)
    interleaved[i] ^= ks[71-i];

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

void vtx256_ambe2_interleave_inversion(char ambe_fr[4][24])
{

  uint32_t silence_check = 0;
  for (int i = 0; i < 20; i++)
  {
    silence_check <<= 1;
    silence_check |= (uint8_t)ambe_fr[0][23-i];
  }

  if (silence_check == 0xF8014)
    goto END_INV;

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

  //invert the bits in the 49 to 71 position
  for (int8_t i = 49; i < 72; i++)
    interleaved[i] ^= 1;

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

  END_INV: ; //do nothing

}