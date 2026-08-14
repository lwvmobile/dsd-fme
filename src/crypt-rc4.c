/*-------------------------------------------------------------------------------
 * rc4.c         Crypthings
 * RC4 Alg
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

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
