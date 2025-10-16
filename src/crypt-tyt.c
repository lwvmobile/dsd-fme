#include "dsd.h"
#include "dmr_const.h"
#include "pc4.h"

//interleaved code words for AMBE+2 (as it arrives over the air)
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

//NOTE: This mode DOES NOT work over a repeater, simplex only
//repeaters may or will attempt to correct the frame errors
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

void tyt_ap_pc4_keystream_creation(dsd_state * state, char * input)
{
  unsigned char key1[64]; memset(key1, 0, sizeof(key1));
  unsigned char key2[64]; memset(key2, 0, sizeof(key2));
  
  char buf[1024];
  strncpy(buf, input, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  
  char *pEnd;
  uint64_t K1 = strtoull(buf, &pEnd, 16);
  uint64_t K2 = strtoull(pEnd, &pEnd, 16);
  uint64_t K3 = strtoull(pEnd, &pEnd, 16);
  uint64_t K4 = strtoull(pEnd, &pEnd, 16);
  
  if (K3 != 0 || K4 != 0) //256-bit keys loaded as ASCII characters
  {
      //start loading K1, K2, K3, and K4 as ASCII to fill 64 bytes of key1
      int k = 0; uint8_t x = 0;

      for (int i = 0; i < 16; i++)
      {
          x = ((K1 >> (60-(i*4))) & 0xF);
          if (x >= 0 && x <= 9) //numbers 0-9
              x += 0x30;  //ASCII representation of numbers is 0x30, 0x31....0x39
          else x += 0x37; //Upper ASCII A,B,C,D,E,F is 0x41, 0x42...0x46
          key1[k++] = x;
      }

      for (int i = 0; i < 16; i++)
      {
          x = ((K2 >> (60-(i*4))) & 0xF);
          if (x >= 0 && x <= 9) //numbers 0-9
              x += 0x30;  //ASCII representation of numbers is 0x30, 0x31....0x39
          else x += 0x37; //Upper ASCII A,B,C,D,E,F is 0x41, 0x42...0x46
          key1[k++] = x;
      }

      for (int i = 0; i < 16; i++)
      {
          x = ((K3 >> (60-(i*4))) & 0xF);
          if (x >= 0 && x <= 9) //numbers 0-9
              x += 0x30;  //ASCII representation of numbers is 0x30, 0x31....0x39
          else x += 0x37; //Upper ASCII A,B,C,D,E,F is 0x41, 0x42...0x46
          key1[k++] = x;
      }

      for (int i = 0; i < 16; i++)
      {
          x = ((K4 >> (60-(i*4))) & 0xF);
          if (x >= 0 && x <= 9) //numbers 0-9
              x += 0x30;  //ASCII representation of numbers is 0x30, 0x31....0x39
          else x += 0x37; //Upper ASCII A,B,C,D,E,F is 0x41, 0x42...0x46
          key1[k++] = x;
      }

      //debug
      // fprintf (stderr, "ASCII: ");
      // for (int i = 0; i < 64; i++)
      //     fprintf (stderr, " %02X", key1[i]);
      // fprintf (stderr, "\n");

      /* Create key schedule */
      create_keys(&ctx, key1, 64);
      ctx.rounds = nbround;

      fprintf(stderr, "DMR TYT AP (PC4) 256-bit Key %016llX%016llX%016llX%016llX with Forced Application\n", 
          (unsigned long long)K1, (unsigned long long)K2, (unsigned long long)K3, (unsigned long long)K4);

      state->tyt_ap = 1;
  }

  else //128-bit loaded as reverse bytes
  {

      u64_to_bytes_be(K1, &key1[0]);
      u64_to_bytes_be(K2, &key1[8]);

      // reverse load the key bytes into key2
      for (int i=0;i<16;i++)
          key2[i] = key1[15-i];
      
      /* Create key schedule */
      create_keys(&ctx, key2, 16);
      ctx.rounds = nbround;
      
      fprintf (stderr,"DMR TYT AP (PC4) 128-bit Key %016llX%016llX with Forced Application\n", 
        (unsigned long long int)K1, (unsigned long long int)K2);

      state->tyt_ap = 1;
  }
  
  
}

void tyt_ep_aes_keystream_creation(dsd_state * state, char * input)
{
  char buf[1024];
  strncpy(buf, input, 1023);
  buf[1023] = '\0';

  char *pEnd;
  unsigned long long int K1 = strtoull (buf, &pEnd, 16);
  unsigned long long int K2 = strtoull (pEnd, &pEnd, 16);

  //
  uint8_t static_key[32];
  memset(static_key, 0, sizeof(static_key));

  //static key value
  static_key[0]=0x6e;  static_key[1]=0x02;  static_key[2]=0x8d;  static_key[3]=0x8a;
  static_key[4]=0xca;  static_key[5]=0xeb;  static_key[6]=0x9b;  static_key[7]=0xbe;
  static_key[8]=0x42;  static_key[9]=0x72;  static_key[10]=0xfb; static_key[11]=0x82;
  static_key[12]=0x64; static_key[13]=0x56; static_key[14]=0x31; static_key[15]=0xfa;

  //the key value provided by user
  uint8_t user_key[16];
  memset(user_key, 0, sizeof(user_key));

  //Load user key into array to manipulate
  for (int i = 0; i < 8; i++)
  {
    user_key[i+0]  = (K1 >> (56-(i*8))) & 0xFF;
    user_key[i+8]  = (K2 >> (56-(i*8))) & 0xFF;
  }

  uint8_t input_register[16];
  memset(input_register, 0, sizeof(input_register));

  //manipulate user provided key by loading bytes in reverse order into the input_register
  for (int i = 0; i < 16; i++)
  input_register[15-i] = user_key[i];

  uint8_t ks_bytes[16];
  memset(ks_bytes, 0, sizeof(ks_bytes));

  //create keystream
  aes_ofb_keystream_output(input_register, static_key, ks_bytes, 0, 1);
  uint8_t ks_bits[128];
  memset(ks_bits, 0, sizeof(ks_bits));
  unpack_byte_array_into_bit_array(ks_bytes, ks_bits, 16);

  //load static keystream into ctx.bits since that isn't ever zeroed out
  for (int i = 0; i < 49; i++)
    ctx.bits[i] = ks_bits[i];

  fprintf (stderr,"DMR TYT EP (AES-128) Key %016llX%016llX with Forced Application\n", K1, K2);
  state->tyt_ep = 1;

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
