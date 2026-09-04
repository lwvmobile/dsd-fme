/*-------------------------------------------------------------------------------
 * rc4.c         Crypthings
 * RC4 Alg
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
#include "bp.h"

//this version is for voice, going to transition to a block output version
void rc4_voice_decrypt(int drop, uint8_t keylength, uint8_t messagelength, uint8_t key[], uint8_t cipher[], uint8_t plain[])
{
  int i, j, count;
  uint8_t t, b;

  //init Sbox
  uint8_t S[256];
  for(int i = 0; i < 256; i++) S[i] = i;

  //Key Scheduling
  j = 0;
  for(i = 0; i < 256; i++)
  {
    j = (j + S[i] + key[i % keylength]) % 256;
    t = S[i];
    S[i] = S[j];
    S[j] = t;
  }

  //Drop Bytes and Cipher Byte XOR
  i = j = 0;
  for(count = 0; count < (messagelength + drop); count++)
  {
    i = (i + 1) % 256;
    j = (j + S[i]) % 256;
    t = S[i];
    S[i] = S[j];
    S[j] = t;
    b = S[(S[i] + S[j]) % 256];

    //return mbe payload byte here
    if (count >= drop)
      plain[count - drop] = b^cipher[count - drop];

  }

}

//this is for PDU usage
void rc4_block_output (int drop, int keylen, int meslen, uint8_t * key, uint8_t * output_blocks)
{
  int i, j, x, count;
  unsigned int keylength = (unsigned int)keylen;
  unsigned int messagelength = (unsigned int)meslen;
  unsigned int S[256];

  for(i=0; i<256; i++)
    S[i] = i;

  j = 0;
  for(i = 0; i<256; i++)
  {
    j = (j + S[i] + key[i % keylength]) % 256;
    unsigned int temp = S[i];
    S[i] = S[j];
    S[j] = temp;
  }

  //Generate Keystream
  i = 0;
  j = 0;
  x = 0;
  unsigned int byte;

  // fprintf (stderr, " Keystream Octets = ");
  for(count = 0; count < (messagelength + drop); count++)
  {
    i = (i + 1) % 256;
    j = (j + S[i]) % 256;
    unsigned int temp = S[i];
    S[i] = S[j];
    S[j] = temp;
    byte = S[(S[i] + S[j]) % 256];

    //Collect Output blocks
    if (count >= drop)
      output_blocks[x++] = byte;

  }

}

//This is now verified to work after changing the drop byte value from 256 to 0.
//also, had to change the application to not skip the additional 7 bits like DMRA or P25 does.
void hytera_enhanced_rc4_setup(dsd_opts * opts, dsd_state * state, unsigned long long int key_value, unsigned long long int mi_value)
{

  UNUSED(opts);
  uint8_t key[5];  memset (key, 0, sizeof(key));
  uint8_t kiv[5];  memset (kiv, 0, sizeof(kiv));
  uint8_t mi[5];   memset (mi, 0, sizeof(mi));
  uint8_t ks[135]; memset (ks, 0, sizeof(ks));

  //load key_value into key array
  key[0] = ((key_value & 0xFF00000000) >> 32UL);
  key[1] = ((key_value & 0xFF000000) >> 24);
  key[2] = ((key_value & 0xFF0000) >> 16);
  key[3] = ((key_value & 0xFF00) >> 8);
  key[4] = ((key_value & 0xFF) >> 0);

  //load mi_value into mi array
  mi[0] = ((mi_value & 0xFF00000000) >> 32UL);
  mi[1] = ((mi_value & 0xFF000000) >> 24);
  mi[2] = ((mi_value & 0xFF0000) >> 16);
  mi[3] = ((mi_value & 0xFF00) >> 8);
  mi[4] = ((mi_value & 0xFF) >> 0);

  //pointer to the ks_octet storage
  uint8_t * ks_octets;
  if (state->currentslot == 0)
    ks_octets = state->ks_octetL;
  else ks_octets = state->ks_octetR;

  //NOTE: Drop Byte value is 0
  rc4_block_output(0, 5, 135, key, ks);

  for (int i = 0; i < 5; i++)
    kiv[i] = key[i] ^ mi[i];

  for (int i = 0; i < 135; i++)
    ks_octets[i] = kiv[i%5] ^ ks[i];

  //debug
  // fprintf (stderr, " KS: ");
  // for (int i = 0; i < 135; i++)
  // {
  //   if ((i != 0) && ((i%7) == 0))
  //     fprintf (stderr, " ");
  //   fprintf (stderr, "%02X", ks[i]); //ks_octets
  // }

  //NULL pointer to ks_octets
  ks_octets = NULL;

  //end line break
  // fprintf (stderr, "\n");

}

void rc_poor_keystream_output(uint8_t * key, uint8_t * ks_bytes)
{
  int16_t i, j, x, count;
  uint8_t t, b;

  //init Sbox
  uint8_t S[255];
  for(i = 0; i < 255; i++)
    S[i] = i;

  //Key Scheduling
  j = 0;
  for(i = 0; i < 255; i++)
  {
    j = (j + S[i] + key[i % 8]) % 255;
    t = S[i];
    S[i] = S[j];
    S[j] = t;
  }

  //KS Byte collection
  i = j = x = 0;
  for(count = 0; count < 6; count++)
  {
    i = (i + 1) % 255;
    j = (j + S[i]) % 255;
    t = S[i];
    S[i] = S[j];
    S[j] = t;
    b = (S[i] + S[j]) % 255;
    ks_bytes[x++] = b;
  }

}

void auctus_keystream_creation(dsd_state * state, char * input)
{

  uint8_t key[8];
  memset (key, 0, sizeof(key));

  uint16_t len = strlen((const char*)input);

  //sanity check on provided char len
  if (len > 8)
    len = 8;

  //load chars as uint8_t into key array
  for (int i = 0; i < len; i++)
    key[i] = (uint8_t)input[i];

  //debug
  fprintf (stderr, "Key String: %s; LEN: %d; HEX: ", input, len);
  for (int i = 0; i < len; i++)
    fprintf (stderr, "%02X", key[i]);
  fprintf (stderr, "\n");

  uint8_t ks_bytes[6];
  memset (ks_bytes, 0, sizeof(ks_bytes));

  rc_poor_keystream_output(key, ks_bytes);

  uint8_t ks_bits[48];
  memset(ks_bits, 0, sizeof(ks_bits));
  unpack_byte_array_into_bit_array(ks_bytes, ks_bits, 6);

  //Reorder the bits of the keystream to compensate
  //for 16-bit words with reversed bit storage
  for (uint16_t j = 0; j < 3; j++)
  {
    uint16_t k = ((j+1)*16) - 1;
    uint16_t x = j*16;
    for (uint16_t i = 0; i < 16; i++)
    {
      state->static_ks_bits[0][x] = ks_bits[k];
      state->static_ks_bits[1][x] = ks_bits[k];

      //debug
      // fprintf (stderr, " K: %02d; ", k);
      
      //debug
      // fprintf (stderr, " X: %02d; ", x);

      k--;
      x++;
    }
  }

  fprintf (stderr,"Auctus Keystream: ");
  for (uint16_t i = 0; i < 6; i++)
    fprintf (stderr, "%02X", ks_bytes[i]);
  fprintf (stderr," - Word Bit Reverse: ");
  for (uint16_t i = 0; i < 6; i++)
    fprintf (stderr, "%02X", convert_bits_into_output(state->static_ks_bits[0]+(i*8), 8));
  fprintf (stderr, " with Forced Application \n");

  state->straight_ks = 1;
  state->straight_mod = 49;

}

void vertex_40_keystream_creation(dsd_state * state, unsigned long long int key_value)
{

  uint8_t key[8];
  memset (key, 0, sizeof(key));

  //load key from key_value
  for (int i = 0; i < 5; i++)
    key[i] = (key_value >> (32-(i*8))) & 0xFF;

  uint8_t rc4_bytes[4];
  memset (rc4_bytes, 0, sizeof(rc4_bytes));

  rc4_block_output(0, 5, 4, key, rc4_bytes);

  uint64_t vtx_key = 0;

  //use the bytes from rc4 output to select 4 BP key 16-bit words
  for (int i = 0; i < 4; i++)
  {
    vtx_key <<= 16;
    vtx_key |= BPK[rc4_bytes[i]];
  }

  fprintf (stderr,"Vertex RC4 Bytes: ");
  for (uint16_t i = 0; i < 8; i++)
    fprintf (stderr, "%02X ", rc4_bytes[i]);
  fprintf (stderr, "Vertex Keystream: %016llX;", (unsigned long long int)vtx_key);
  fprintf (stderr, "\n");

  uint8_t vtx_bytes[8];
  memset (vtx_bytes, 0, sizeof(vtx_bytes));

  for (int i = 0; i < 8; i++)
    vtx_bytes[i] = (vtx_key >> (56-(i*8))) & 0xFF;

  unpack_byte_array_into_bit_array(vtx_bytes, state->static_ks_bits[0], 8);
  unpack_byte_array_into_bit_array(vtx_bytes, state->static_ks_bits[1], 8);

  state->vtx_key_loaded = 1;

}

void vertex_keystream_setup(dsd_state * state, char * input)
{
  uint16_t len = 0;
  char * curr;
  curr = strtok(input, ":"); //should be len (mod) of key (decimal)
  if (curr != NULL)
    sscanf (curr, "%hd", &len);
  else goto END_KS;

  //len sanity check
  if (len != 0 && len != 10 && len != 64)
  {
    fprintf (stderr, "Vertex Standard Invalid Key Len Specified;");
    goto END_KS;
  }

  curr = strtok(NULL, ":"); //should be key in hex
  if (curr == NULL)
  {
    fprintf (stderr, "Vertex Standard No Key Value Specified;");
    goto END_KS;
  }

  if (len == 10)
  {
    sscanf (curr, "%llx", &state->vtx40_key);
    vertex_40_keystream_creation(state, state->vtx40_key);
  }

  else if (len == 64)
  {
    memcpy(state->vtx256_key, curr, 64);
    vertex_256_keystream_creation(state, curr);
  }

  //special use case 0:0 to do the inversion, but the keystream of zero to clear the errors
  //essentially no keystream is applied, but to analyze the frames, clean up the errors
  else if (len == 0)
  {
    memset(state->static_ks_bits, 0, sizeof(state->static_ks_bits));
    state->vtx256_ekey = 1;
  }

  END_KS: ; //do nothing

}
