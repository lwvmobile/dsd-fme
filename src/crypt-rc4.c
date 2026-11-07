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
void hytera_enhanced_rc4_setup(dsd_opts * opts, dsd_state * state, unsigned long long int key_value, unsigned long long int mi_value, int len)
{

  UNUSED(opts);
  uint8_t key[5];   memset (key, 0, sizeof(key));
  uint8_t kiv[5];   memset (kiv, 0, sizeof(kiv));
  uint8_t mi[5];    memset (mi, 0, sizeof(mi));
  uint8_t ks[2000]; memset (ks, 0, sizeof(ks));

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
  rc4_block_output(0, 5, len, key, ks);

  for (int i = 0; i < 5; i++)
    kiv[i] = key[i] ^ mi[i];

  for (int i = 0; i < len; i++)
    ks_octets[i] = kiv[i%5] ^ ks[i];

  //debug
  // fprintf (stderr, " KS: ");
  // for (int i = 0; i < len; i++)
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

  #ifdef VERTEX_KEY40_BY_TG
  //skip constant verbosity on keyloading
  #else
  fprintf (stderr,"Vertex RC4 Bytes: ");
  for (uint16_t i = 0; i < 8; i++)
    fprintf (stderr, "%02X ", rc4_bytes[i]);
  fprintf (stderr, "Vertex Keystream: %016llX;", (unsigned long long int)vtx_key);
  fprintf (stderr, "\n");
  #endif

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
    fprintf (stderr, "Vertex Standard 40-bit Key: %010llX \n", state->vtx40_key);
    vertex_40_keystream_creation(state, state->vtx40_key);
  }

  else if (len == 64)
  {
    memcpy(state->vtx256_key, curr, 64);
    fprintf (stderr, "Vertex Standard 256-bit Key: ");
    for (int i = 0; i < 64; i++)
      fprintf (stderr, "%c", state->vtx256_key[i]);
    fprintf (stderr, "\n");
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

//https://patents.google.com/patent/CN106789903B/en
void hytera_ota_keystream_setup(dsd_state * state, char * input)
{

  uint16_t type = 0;
  char * curr;
  curr = strtok(input, ":");
  if (curr != NULL)
    sscanf (curr, "%hd", &type);
  else goto END_KS;

  //type sanity check
  if (type != 0 && type != 128)
  {
    fprintf (stderr, "Hytera OTA Encryption Invalid Key type Specified;");
    goto END_KS;
  }

  curr = strtok(NULL, ":"); //should be key in hex
  if (curr == NULL)
  {
    fprintf (stderr, "Hytera OTA Encryption No Key Value Specified;");
    goto END_KS;
  }

  if (type == 0) //No Key specified, just check for potential HOTA and DMRA EP
  {
    fprintf (stderr, "Hytera OTA Encryption Check w/ DMRA EP Late Entry IV Check;\n");
    state->hota_enc_check = 1;
  }
  else //if (type == 128) //RC4 setup on keystream
  {
    uint8_t key[50];
    memset(key, 0, sizeof(key));

    parse_raw_user_string(curr, key);

    // #ifdef DEBUG_HOTA
    fprintf(stderr, "Hytera OTA Encryption RC4 Key: ");
    for (int i = 0; i < 16; i++)
      fprintf (stderr, "%02X", key[i]);
    fprintf (stderr, "\n");
    // #endif

    for (int i = 0; i < 16; i++)
      state->hota_rc4_key[i] = key[i];

    uint8_t ks_bytes[80];
    memset(ks_bytes, 0, sizeof(ks_bytes));

    //NOTE: Drop Byte value is 0
    rc4_block_output(0, 16, 80, key, ks_bytes);

    #ifdef DEBUG_HOTA
    for (int j = 0; j < 5; j++)
    {
      fprintf(stderr, "Hytera OTA Encryption RC4  KS: ");
      for (int i = 0; i < 16; i++)
        fprintf (stderr, "%02X", ks_bytes[i+j*16]);
      fprintf (stderr, "\n");
    }
    #endif

    for (int i = 0; i < 80; i++)
      state->hota_rc4_ks[i] = ks_bytes[i];

    //configure individual byte and full 96-bit PDU bit final bit shift / rotations
    for (int i = 0; i < 12; i++)
      state->hota_byte_shifts[i] = ks_bytes[i+16] % 8;

    state->hota_bit_shift  =  ks_bytes[29];
    state->hota_bit_shift <<= 8;
    state->hota_bit_shift |=  ks_bytes[28];
    state->hota_bit_shift %=  96;

    #ifdef DEBUG_HOTA
    fprintf(stderr, "Hytera OTA Full Bit Shift: %d; Hytera OTA Byte Shifts: ", state->hota_bit_shift);
    for (int i = 0; i < 12; i++)
      fprintf (stderr, "%d,", state->hota_byte_shifts[i]);
    fprintf (stderr, "\n");
    #endif

    state->hota_key_loaded = 1;
    state->dmr_is_hota = 1;
  }

  END_KS: //

}

void rsr_rot(uint8_t * input, int16_t bitlen)
{
  uint8_t bit = input[bitlen-1];
  for (int16_t i = (bitlen-1); i > 0; i--)
    input[i] = input[i-1];
  input[0] = bit;
}

void lsr_rot(uint8_t * input, int16_t bitlen)
{
  uint8_t bit = input[0];
  for (int16_t i = 0; i < (bitlen-1); i++)
    input[i] = input[i+1];
  input[(bitlen-1)] = bit;
}

void hytera_ota_decrypt_bptc_96(dsd_state * state, uint8_t * input, uint8_t type)
{

  uint8_t bits[96];
  memset(bits, 0, sizeof(bits));

  uint8_t bytes[12];
  memset(bytes, 0, sizeof(bytes));

  for (int i = 0; i < 96; i++)
    bits[i] = input[i];

  #ifdef DEBUG_HOTA
  fprintf (stderr, "\n\n-----BPTC 96 Bit Decryption-----");
  #endif

  #ifdef DEBUG_HOTA
  fprintf (stderr, "\n");
  fprintf(stderr, "HOTA Inp: ");
  for (int i = 0; i < 12; i++)
    fprintf (stderr, "%02X", convert_bits_into_output(bits+i*8, 8));
  #endif

  //RC4 key type decryption
  if (state->hota_key_loaded == 1)
  {

    //reverse the final bit rotation with a right shift register
    for (int i = 0; i < state->hota_bit_shift; i++)
      rsr_rot(bits, 96);

    //reverse the individual bit rotations in each byte with a right shift register
    for (int i = 0; i < 12; i++)
    {
      int shift = state->hota_byte_shifts[i];
      for (int j = 0; j < shift; j++)
        rsr_rot(bits+(i*8), 8);
    }

    pack_bit_array_into_byte_array(bits, bytes, 12);

    #ifdef DEBUG_HOTA
    fprintf (stderr, "\n");
    fprintf(stderr, "HOTA Rot: ");
    for (int i = 0; i < 12; i++)
      fprintf (stderr, "%02X", bytes[i]);
    #endif

    //create a new RC4 keystream derivitive with preconfigured HOTA Keystream
    uint8_t key[19];
    memset(key, 0, sizeof(key));
    for (int i = 0; i < 16; i++)
      key[i] = state->hota_rc4_key[i];

    int plen =  9;
    int blen = 72;
    int klen = 16;
    int k = type;

    //VLC or TLC w/ RS encoding
    if (type == 1 || type == 2)
    {
      key[klen]    = (uint8_t)convert_bits_into_output(bits+blen, 8);
      key[klen++] ^= state->hota_rc4_ks[k++ % 16];
      blen += 8;
      key[klen]    = (uint8_t)convert_bits_into_output(bits+blen, 8);
      key[klen++] ^= state->hota_rc4_ks[k++ % 16];
      blen += 8;
      key[klen]    = (uint8_t)convert_bits_into_output(bits+blen, 8);
      key[klen++] ^= state->hota_rc4_ks[k++ % 16];
      blen += 8;
    }
    else //CRC16
    {
      plen = 10;
      blen = 80;
      key[klen]    = (uint8_t)convert_bits_into_output(bits+blen, 8);
      key[klen++] ^= state->hota_rc4_ks[k++ % 16];
      blen += 8;
      key[klen]    = (uint8_t)convert_bits_into_output(bits+blen, 8);
      key[klen++] ^= state->hota_rc4_ks[k++ % 16];
      blen += 8;
    }

    #ifdef DEBUG_HOTA
    fprintf (stderr, "\n");
    fprintf(stderr, "HOTA KEY: ");
    for (int i = 0; i < klen; i++)
      fprintf (stderr, "%02X", key[i]);
    #endif

    uint8_t ks_bytes[12];
    memset(ks_bytes, 0, sizeof(ks_bytes));

    //NOTE: Drop Byte value is 0
    rc4_block_output(0, klen, plen, key, ks_bytes);

    #ifdef DEBUG_HOTA
    fprintf (stderr, "\n");
    fprintf(stderr, "HOTA RC4: ");
    for (int i = 0; i < plen; i++)
      fprintf (stderr, "%02X", ks_bytes[i]);
    #endif

    //reset the klen pointer to the start of the RS or CRC encoding
    klen -= (12 - plen);

    //decrypt final bytes
    for (int i = 0; i < plen; i++)
      bytes[i] ^= ks_bytes[i];

    //Add current RS or CRC encoding
    for (int i = plen; i < 12; i++)
      bytes[i] = key[klen++];

    // #ifdef DEBUG_HOTA
    // fprintf (stderr, "\n");
    // fprintf(stderr, "HOTA Type: %X; PLEN: %d; BLEN: %d; KLEN: %d; K: %d;", type, plen, blen, klen, k);
    // #endif

    //unpack the final decrypted bytes back into a bit array
    unpack_byte_array_into_bit_array(bytes, input, 12);

  }

  #ifdef DEBUG_HOTA
  fprintf (stderr, "\n");
  fprintf(stderr, "HOTA Out: ");
  for (int i = 0; i < 12; i++)
    fprintf (stderr, "%02X", bytes[i]);
  fprintf (stderr, "\n");
  #endif

}

static const uint8_t elc_row_map[77] = {
    /* R0: (h0 << 3) | (h1 >> 5) */
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,

    /* R1: ((h1 & 0x1F) << 6) | (h2 >> 2) */
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,

    /* R2: ((h2 & 0x03) << 9) | (h3 << 1) | CRC[4] */
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 72,

    /* R3: (h4 << 3) | ((h5 >> 6) << 1) | CRC[3] */
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 73,

    /* R4: ((h5 & 0x3F) << 5) | ((h6 >> 4) << 1) | CRC[2] */
    42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 74,

    /* R5: ((h6 & 0x0F) << 7) | ((h7 >> 2) << 1) | CRC[1] */
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 75,

    /* R6: ((h7 & 0x03) << 9) | (h8 << 1) | CRC[0] */
    62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 76
};

void hytera_create_lc_ctx(dsd_state * state, uint8_t * input)
{

  #ifdef DEBUG_HOTA
  fprintf (stderr, "\n-----Voice LC 4 Byte CTX-----");
  #endif

  uint8_t bits[77];
  memset(bits, 0, sizeof(bits));

  for (int i = 0; i < 72; i++)
    bits[i] = input[i] & 1;

  /* Mode 1: left-rotate LC by 1 bit before CRC and rows */
  // if (mode) //never been observed, so disabling this
  //   lsr_rot(bits, 72);

  /* CRC5 over the 9 bytes */
  uint32_t crc5 = 0;
  for (int i = 0; i < 9; i++)
    crc5 += convert_bits_into_output(bits + (i * 8), 8);
  crc5 %= 31;

  for (int i = 0; i < 5; i++)
    bits[72 + i] = (crc5 >> (4 - i)) & 1;

  uint16_t rows[7] = {0};
  for (int i = 0; i < 77; i++) {
    rows[i / 11] <<= 1;
    rows[i / 11]  |= bits[elc_row_map[i]] & 1;
  }

  #ifdef DEBUG_HOTA
  fprintf (stderr, "\nHOTA ROW: ");
  for (int i = 0; i < 7; i++)
    fprintf (stderr, "%03X ", rows[i]);
  #endif

  uint16_t q0 = rows[3] & 0x7FF;
  uint16_t q1 = 0;
  for (int i = 0; i < 7; i++)
    q1 ^= rows[i];
  q1 &= 0x7FF;

  uint8_t ctx_out[4];
  ctx_out[0] = (uint8_t)(q0 >> 3);
  ctx_out[1] = (uint8_t)((q0 & 7) << 5);
  ctx_out[2] = (uint8_t)(q1 >> 3);
  ctx_out[3] = (uint8_t)((q1 & 7) << 5);

  //load ctx to state
  for (int i = 0; i < 4; i++)
    state->hota_lc_ctx[state->currentslot][i] = ctx_out[i];

  //signal ctx is available for embedded lc
  state->hota_ctx_ok[state->currentslot] = 1; //disable to test the search

  #ifdef DEBUG_HOTA
  fprintf (stderr, "\nHOTA CTX: Q0: %03X; Q1: %03X; CTX: %02X %02X %02X %02X;\n", q0, q1, ctx_out[0], ctx_out[1], ctx_out[2], ctx_out[3]);
  #endif
}

static const uint8_t elc_transpose_map[32] = {
     0,  4,  8, 12, 16, 20, 24, 28,   /* → result bits 31..24 */
     1,  5,  9, 13, 17, 21, 25, 29,   /* → result bits 23..16 */
     2,  6, 10, 14, 18, 22, 26, 30,   /* → result bits 15.. 8 */
     3,  7, 11, 15, 19, 23, 27, 31    /* → result bits  7.. 0 */
};

void hytera_ota_decrypt_emb_lc(dsd_state * state, uint8_t input[8][16])
{

  if (state->hota_ctx_ok[state->currentslot] == 0)
    return;

  #ifdef DEBUG_HOTA
  fprintf (stderr, "\n-----EMB LC and FEC Decryption-----\n");
  #endif

  uint8_t bits[128];
  memset(bits, 0, sizeof(bits));

  uint8_t bytes[16];
  memset(bytes, 0, sizeof(bytes));

  int k = 0;
  #ifdef DEBUG_HOTA
  fprintf(stderr, "HOTA Inp: ");
  for (int i = 0; i < 8; i++)
    fprintf (stderr, "%04X", (uint16_t)convert_bits_into_output(input[i], 16));
  fprintf (stderr, "\n");
  #endif

  //OTA load as linear bits
  for (int i = 0; i < 16; i++)
  {
    for (int j = 0; j < 8; j++)
      bits[k++] = input[j][i];
  }

  pack_bit_array_into_byte_array(bits, bytes, 16);

  #ifdef DEBUG_HOTA
  fprintf(stderr, "HOTA Col: ");
  for (int i = 0; i < 16; i++)
    fprintf (stderr, "%02X", bytes[i]);
  fprintf (stderr, "\n");
  #endif

  //RC4 things
  uint8_t key[20];
  memset(key, 0, sizeof(key));

  uint8_t ks_bytes[16];
  memset(ks_bytes, 0, sizeof(ks_bytes));

  k = 0;
  for (int i = 0; i < 16; i++)
    key[k++] = state->hota_rc4_key[i];

  for (int i = 0; i < 4; i++)
    key[k++] = state->hota_lc_ctx[state->currentslot][i];

  #ifdef DEBUG_HOTA
  fprintf(stderr, "HOTA Key: ");
  for (int i = 0; i < 20; i++)
    fprintf (stderr, "%02X", key[i]);
  fprintf (stderr, "\n");
  #endif

  //NOTE: Drop Byte value is 0
  rc4_block_output(0, 20, 16, key, ks_bytes);

  #ifdef DEBUG_HOTA
  fprintf(stderr, "HOTA RC4: ");
  for (int i = 0; i < 12; i++)
    fprintf (stderr, "%02X", ks_bytes[i]);
  fprintf (stderr, "\n");
  #endif

  //Nibble swap from state->hota_rc4_ks
  ks_bytes[1]  &= 0xF0;
  ks_bytes[1]  |= state->hota_rc4_ks[30] & 0xF;

  ks_bytes[3]  &= 0xF0;
  ks_bytes[3]  |= state->hota_rc4_ks[31] & 0xF;

  ks_bytes[5]  &= 0xF0;
  ks_bytes[5]  |= state->hota_rc4_ks[32] & 0xF;

  ks_bytes[7]  &= 0xF0;
  ks_bytes[7]  |= state->hota_rc4_ks[33] & 0xF;

  ks_bytes[9]  &= 0xF0;
  ks_bytes[9]  |= state->hota_rc4_ks[34] & 0xF;

  ks_bytes[11] &= 0xF0;
  ks_bytes[11] |= state->hota_rc4_ks[35] & 0xF;

  #ifdef DEBUG_HOTA
  fprintf(stderr, "HOTA Nib: ");
  for (int i = 0; i < 6; i++)
    fprintf (stderr, "   %X", state->hota_rc4_ks[i+30] & 0xF);
  fprintf (stderr, "\n");
  #endif

  #ifdef DEBUG_HOTA
  fprintf(stderr, "HOTA RC4: ");
  for (int i = 0; i < 12; i++)
    fprintf (stderr, "%02X", ks_bytes[i]);
  fprintf (stderr, "\n");
  #endif

  //M0, M1, M2 values 
  uint8_t ks_bits[128];
  memset(ks_bits, 0, sizeof(ks_bits));
  unpack_byte_array_into_bit_array(ks_bytes, ks_bits, 16);

  uint8_t int_bits[128];
  memset(int_bits, 0, sizeof(int_bits));

  k = 0;
  for (int i = 0; i < 3; i++)
  {
    for (int j = 0; j < 32; j++)
    int_bits[k++] = ks_bits[elc_transpose_map[j]+(i*32)];
  }

  uint8_t int_bytes[16];
  memset(int_bytes, 0, sizeof(int_bytes));

  pack_bit_array_into_byte_array(int_bits, int_bytes, 16);

  #ifdef DEBUG_HOTA
  fprintf(stderr, "HOTA Int: ");
  for (int i = 0; i < 12; i++)
    fprintf (stderr, "%02X", int_bytes[i]);
  fprintf (stderr, "\n");
  #endif

  //reorder the int_bytes
  memset(ks_bytes, 0, sizeof(ks_bytes));

  k = 0;
  for (int i = 0; i < 4; i++)
    ks_bytes[k++] = int_bytes[i];
  for (int i = 0; i < 4; i++)
    ks_bytes[k++] = int_bytes[i+4];
  for (int i = 0; i < 4; i++)
    ks_bytes[k++] = int_bytes[i+4];
  for (int i = 0; i < 4; i++)
    ks_bytes[k++] = int_bytes[i+8];

  for (int i = 0; i < 16; i++)
    bytes[i] ^= ks_bytes[i];

  #ifdef DEBUG_HOTA
  fprintf(stderr, "HOTA KS2: ");
  for (int i = 0; i < 16; i++)
    fprintf (stderr, "%02X", ks_bytes[i]);
  fprintf (stderr, "\n");
  #endif

  #ifdef DEBUG_HOTA
  fprintf(stderr, "HOTA Out: ");
  for (int i = 0; i < 16; i++)
    fprintf (stderr, "%02X", bytes[i]);
  fprintf (stderr, "\n\n");
  #endif

  unpack_byte_array_into_bit_array(bytes, bits, 16);

  k = 0;

  //OTA reload from linear bits to 2d input array
  for (int i = 0; i < 16; i++)
  {
    for (int j = 0; j < 8; j++)
      input[j][i] = bits[k++];
  }

}

//This is a modified decrypt but to brute force an unknown 22-bit ctx value
//This should not be run on live systems, but useful for replay or debug
//This takes around 25 seconds on a full search and failure on Ryzen 5 2600
void hytera_ota_search_emb_lc(dsd_state * state, uint8_t input[8][16])
{

  #ifdef DEBUG_HOTA
  fprintf (stderr, "\n-----EMB LC and FEC CTX Search-----\n");
  #endif

  for (uint32_t ctx = 0; ctx < 0x3FFFFF; ctx++)
  {

    uint8_t bits[128];
    memset(bits, 0, sizeof(bits));

    uint8_t bytes[16];
    memset(bytes, 0, sizeof(bytes));

    int k = 0;

    //OTA load as linear bits
    for (int i = 0; i < 16; i++)
    {
      for (int j = 0; j < 8; j++)
        bits[k++] = input[j][i];
    }

    pack_bit_array_into_byte_array(bits, bytes, 16);

    uint16_t q0 = 0, q1 = 0;
    q0 = (ctx >> 11) & 0x7FF;
    q1 = (ctx >>  0) & 0x7FF;

    uint8_t ctx_out[4];
    memset(ctx_out, 0, sizeof(ctx_out));

    ctx_out[0] = (uint8_t)(q0 >> 3);
    ctx_out[1] = (uint8_t)((q0 & 7) << 5);
    ctx_out[2] = (uint8_t)(q1 >> 3);
    ctx_out[3] = (uint8_t)((q1 & 7) << 5);

    //RC4 things
    uint8_t key[20];
    memset(key, 0, sizeof(key));

    uint8_t ks_bytes[16];
    memset(ks_bytes, 0, sizeof(ks_bytes));

    k = 0;
    for (int i = 0; i < 16; i++)
      key[k++] = state->hota_rc4_key[i];

    for (int i = 0; i < 4; i++)
      key[k++] = ctx_out[i];

    //NOTE: Drop Byte value is 0
    rc4_block_output(0, 20, 16, key, ks_bytes);

    //Nibble swap from state->hota_rc4_ks
    ks_bytes[1]  &= 0xF0;
    ks_bytes[1]  |= state->hota_rc4_ks[30] & 0xF;

    ks_bytes[3]  &= 0xF0;
    ks_bytes[3]  |= state->hota_rc4_ks[31] & 0xF;

    ks_bytes[5]  &= 0xF0;
    ks_bytes[5]  |= state->hota_rc4_ks[32] & 0xF;

    ks_bytes[7]  &= 0xF0;
    ks_bytes[7]  |= state->hota_rc4_ks[33] & 0xF;

    ks_bytes[9]  &= 0xF0;
    ks_bytes[9]  |= state->hota_rc4_ks[34] & 0xF;

    ks_bytes[11] &= 0xF0;
    ks_bytes[11] |= state->hota_rc4_ks[35] & 0xF;

    //M0, M1, M2 values 
    uint8_t ks_bits[128];
    memset(ks_bits, 0, sizeof(ks_bits));
    unpack_byte_array_into_bit_array(ks_bytes, ks_bits, 16);

    uint8_t int_bits[128];
    memset(int_bits, 0, sizeof(int_bits));

    k = 0;
    for (int i = 0; i < 3; i++)
    {
      for (int j = 0; j < 32; j++)
      int_bits[k++] = ks_bits[elc_transpose_map[j]+(i*32)];
    }

    uint8_t int_bytes[16];
    memset(int_bytes, 0, sizeof(int_bytes));

    pack_bit_array_into_byte_array(int_bits, int_bytes, 16);

    //reorder the int_bytes
    memset(ks_bytes, 0, sizeof(ks_bytes));

    k = 0;
    for (int i = 0; i < 4; i++)
      ks_bytes[k++] = int_bytes[i];
    for (int i = 0; i < 4; i++)
      ks_bytes[k++] = int_bytes[i+4];
    for (int i = 0; i < 4; i++)
      ks_bytes[k++] = int_bytes[i+4];
    for (int i = 0; i < 4; i++)
      ks_bytes[k++] = int_bytes[i+8];

    for (int i = 0; i < 16; i++)
      bytes[i] ^= ks_bytes[i];

    unpack_byte_array_into_bit_array(bytes, bits, 16);

    //FEC test on results, break and set ctx if good and unique (only one good?)
    uint8_t matrix[8][16];
    memset(matrix, 0, sizeof(matrix));

    //OTA reload from linear bits to 2d input array
    k = 0;
    for (int i = 0; i < 16; i++)
    {
      for (int j = 0; j < 8; j++)
        matrix[j][i] = bits[k++];
    }

    //This will not correct, but reports all errors (hamming corrections and parity)
    uint32_t errors = BPTC_128x77_Check_Errors(matrix);

    if (errors == 0)
    {

      //Extract + CRC5 to validate
      uint8_t lc77[77];
      if (BPTC_128x77_Extract_Data(matrix, lc77) == 0 &&
          ComputeCrc5Bit(lc77) == ConvertBitIntoBytes(&lc77[72], 5))
      {

        #ifdef DEBUG_HOTA
        fprintf (stderr, "CTX: %06X; Q0: %03X; Q1: %03X;\n", ctx, q0, q1);
        fprintf (stderr, "ERR: %d; CRC EXT: %02X; CRC CMP: %02X\n", errors, ComputeCrc5Bit(lc77), ConvertBitIntoBytes(&lc77[72], 5));
        fprintf(stderr, "HOTA Out: ");
        for (int i = 0; i < 16; i++)
          fprintf (stderr, "%02X", bytes[i]);
        fprintf (stderr, "\n\n");
        #endif

        //set missing CTX here, set flag for okay ctx, and return
        for (int i = 0; i < 4; i++)
          state->hota_lc_ctx[state->currentslot][i] = ctx_out[i];
        state->hota_ctx_ok[state->currentslot] = 1;
        return;
      }

    }

    //get out of brute force loop early on exit signal
    if (exitflag)
      break;

  }

}

void hytera_ota_unit_testing(dsd_state * state)
{
  char keystr[100];
  memset(keystr, 0, sizeof(keystr));
  sprintf (keystr, "%s", "2:17bfe59b29a06136fb7d3811fd1a4904");
  hytera_ota_keystream_setup(state, keystr);

  char string[30]; 
  memset(string, 0, sizeof(string));

  uint8_t bytes[12];
  uint8_t bits[96];

  //PI header
  memset(string, 0, sizeof(string));
  sprintf(string, "%s", "2ca5f7b049c350664cf09e4e");
  memset(bytes, 0, sizeof(bytes));
  memset(bits, 0, sizeof(bits));
  parse_raw_user_string(string, bytes);
  unpack_byte_array_into_bit_array(bytes, bits, 12);
  hytera_ota_decrypt_bptc_96(state, bits, 0);
  fprintf (stderr, "\n");

  //VLC
  memset(string, 0, sizeof(string));
  sprintf(string, "%s", "de4998491fe4da822d5d5568");
  memset(bytes, 0, sizeof(bytes));
  memset(bits, 0, sizeof(bits));
  parse_raw_user_string(string, bytes);
  unpack_byte_array_into_bit_array(bytes, bits, 12);
  hytera_ota_decrypt_bptc_96(state, bits, 1);
  fprintf (stderr, "\n");

  //TLC
  memset(string, 0, sizeof(string));
  sprintf(string, "%s", "012f658c3f2a70a8d4766a95");
  memset(bytes, 0, sizeof(bytes));
  memset(bits, 0, sizeof(bits));
  parse_raw_user_string(string, bytes);
  unpack_byte_array_into_bit_array(bytes, bits, 12);
  hytera_ota_decrypt_bptc_96(state, bits, 2);
  fprintf (stderr, "\n");

  //CSBK
  memset(string, 0, sizeof(string));
  sprintf(string, "%s", "1053505ed9b4ff091b36a5c3");
  memset(bytes, 0, sizeof(bytes));
  memset(bits, 0, sizeof(bits));
  parse_raw_user_string(string, bytes);
  unpack_byte_array_into_bit_array(bytes, bits, 12);
  hytera_ota_decrypt_bptc_96(state, bits, 3);
  fprintf (stderr, "\n");

  //data header
  memset(string, 0, sizeof(string));
  sprintf(string, "%s", "cba5da41f9fb252ca859ff89");
  memset(bytes, 0, sizeof(bytes));
  memset(bits, 0, sizeof(bits));
  parse_raw_user_string(string, bytes);
  unpack_byte_array_into_bit_array(bytes, bits, 12);
  hytera_ota_decrypt_bptc_96(state, bits, 6);
  fprintf (stderr, "\n");

  //single burst encoding
  memset(bits, 0, sizeof(bits));
  uint16_t sb_hex = 0x14; //key id 0x14 like in sample
  sb_hex <<= 3;
  sb_hex |= 0x01; //alg 0x01 (RC4)

  //05030A03 is key id 1, alg 1 (009)
  //HOTA SB INT: 05030A03
  //HOTA SB DEI: 01310131
  //SB: 00000001001 - 009; 
  //Slot 2 DMR LE SB ALG ID: 21; KEY ID: 01; RC4;

  //debug 
  // sb_hex = 0x009;

  //load into bit array
  for (int i = 0; i < 11; i++)
    bits[i] = (sb_hex >> (10-i)) & 1;

  //hamming 16_11_4 encode
  uint8_t encoded[32];
  memset(encoded, 0, sizeof(encoded));
  Hamming_16_11_4_encode(bits, encoded);

  //Parity Bit Row (even) --> just duplicate the first 16
  for (int i = 0; i < 16; i++)
    encoded[i+16] = encoded[i];

  //NOTE: For RC, just invert each bit instead
  // for (int i = 0; i < 16; i++)
  //   encoded[i+16] = encoded[i] ^ 1;

  //placement and interleaving
  uint8_t interleave[32];
  memset(interleave, 0, sizeof(interleave));

  const uint8_t sbrc_int[32] =
  {
    0,24,1,25,2,26,3,27,4,28,5,29,6,30,7,31,8,
    16,9,17,10,18,11,19,12,20,13,21,14,22,15,23
  };

  for (int i = 0; i < 32; i++)
    interleave[i] = encoded[sbrc_int[i]];

  fprintf (stderr, "\n");
  fprintf (stderr, "EMB RC / SB HEX: %03X;", sb_hex);
  fprintf (stderr, "\n");
  fprintf (stderr, "Hamming Encoded: %04X", convert_bits_into_output(encoded, 16));
  fprintf (stderr, "\n");
  fprintf (stderr, "Even PB Encoded: %08X", convert_bits_into_output(encoded, 32));
  fprintf (stderr, "\n");
  fprintf (stderr, "CAI Interleaved: %08X", convert_bits_into_output(interleave, 32));

  exitflag = 1;

}

//Apply KS to the Single Burst or Reverse Channel using indexed values.
void hytera_ota_decrypt_sb_rc(dsd_state * state, uint8_t * input)
{

  if (state->hota_key_loaded == 0)
    return;
  else if (state->hota_sbrc_idx1[state->currentslot] == -1)
    return;
  else if (state->hota_sbrc_idx2[state->currentslot] == -1)
    return;

  uint8_t bits[32];
  memset(bits, 0, sizeof(bits));

  //check for empty field (NULL message), return if empty
  if (memcmp(bits, input, 32) == 0)
    return;

  #ifdef DEBUG_HOTA
  fprintf (stderr, "\n\n-----SB_RC and FEC Decryption-----");
  #endif

  for (int i = 0; i < 32; i++)
    bits[i] = input[i] & 1;

  uint8_t bytes[4];
  pack_bit_array_into_byte_array(bits, bytes, 4);

  #ifdef DEBUG_HOTA
  fprintf(stderr, "\n");
  fprintf(stderr, "HOTA SB Inp: ");
  for (int i = 0; i < 4; i++)
    fprintf(stderr, "%02X", bytes[i]);
  fprintf(stderr, "\n");
  #endif

  static const uint8_t sbrc_int[32] = {
    0,24,1,25,2,26,3,27,4,28,5,29,6,30,7,31,8,
    16,9,17,10,18,11,19,12,20,13,21,14,22,15,23
  };

  uint8_t dei[32];
  for (int i = 0; i < 32; i++)
    dei[sbrc_int[i]] = bits[i];

  for (int i = 0; i < 32; i++)
    bits[i] = dei[i];

  pack_bit_array_into_byte_array(bits, bytes, 4);

  #ifdef DEBUG_HOTA
  fprintf(stderr, "HOTA SB Dei: ");
  for (int i = 0; i < 4; i++)
    fprintf(stderr, "%02X", bytes[i]);
  fprintf(stderr, "\n");
  #endif

  //idx values for KS based on calculation or prior search
  int idx1 = state->hota_sbrc_idx1[state->currentslot];
  int idx2 = state->hota_sbrc_idx2[state->currentslot];

  //The index values are always hardset according to test w/ multiple keys
  uint16_t S = (uint16_t)state->hota_rc4_ks[idx1+0] | ((uint16_t)state->hota_rc4_ks[idx1+1] << 8);
  uint16_t Q = ((uint16_t)state->hota_rc4_ks[idx2+0] << 8) | (uint16_t)state->hota_rc4_ks[idx2+1];

  uint16_t U = ((uint16_t)bytes[0] << 8) | bytes[1];
  uint16_t V = ((uint16_t)bytes[2] << 8) | bytes[3];

  #ifdef DEBUG_HOTA
  fprintf(stderr, "HOTA SB UV:  U=%04X V=%04X\n", U, V);
  #endif

  //apply keystream
  V ^= S;
  U ^= Q;

  #ifdef DEBUG_HOTA
  fprintf(stderr, "HOTA SB Dec: U=%04X V=%04X idx1=%u idx2=%u S=%04X Q=%04X\n", U, V, idx1, idx2, S, Q);
  #endif

  //swap endianness of U and V for unpacking 
  bytes[0] = (uint8_t)(U >> 8);
  bytes[1] = (uint8_t)(U & 0xFF);
  bytes[2] = (uint8_t)(V >> 8);
  bytes[3] = (uint8_t)(V & 0xFF);

  #ifdef DEBUG_HOTA
  fprintf(stderr, "HOTA SB Dec: ");
  for (int i = 0; i < 4; i++)
    fprintf(stderr, "%02X", bytes[i]);
  fprintf(stderr, "\n");
  #endif

  unpack_byte_array_into_bit_array(bytes, bits, 4);
  for (int i = 0; i < 32; i++)
    input[i] = bits[sbrc_int[i]];

  pack_bit_array_into_byte_array(input, bytes, 4);
  #ifdef DEBUG_HOTA
  fprintf(stderr, "HOTA SB Out: ");
  for (int i = 0; i < 4; i++)
    fprintf(stderr, "%02X", bytes[i]);
  fprintf(stderr, "\n");
  #endif
}

//UPDATE: So, does appear that this field changes depending on other factors
//not key related directly, but could be CTX related, or RID or similar,
//code has been updated appropriately to continue the search on this
//its possible that idx1 is still always 36, idx2 is variable
void hytera_ota_search_sb_rc(dsd_state * state, uint8_t * input, int ptype)
{

  if (state->hota_key_loaded == 0)
    return;
  else if (ptype > 1)
    return;

  uint8_t bits[32];
  memset (bits, 0, sizeof(bits));

  //check for empty field (NULL message), return if empty
  if (memcmp(bits, input, 32) == 0)
    return;

  #ifdef DEBUG_HOTA
  fprintf (stderr, "\n\n-----SB_RC KS Index Search-----");
  #endif

  static const uint8_t sbrc_int[32] = {
    0,24,1,25,2,26,3,27,4,28,5,29,6,30,7,31,8,
    16,9,17,10,18,11,19,12,20,13,21,14,22,15,23
  };

  for (int i = 0; i < 32; i++)
    bits[sbrc_int[i]] = input[i];

  uint8_t bytes[4];
  memset(bytes, 0, sizeof(bytes));

  pack_bit_array_into_byte_array(bits, bytes, 4);

  uint16_t S = 0, Q = 0, U = 0, V = 0, Y = 0, Z = 0;

  uint8_t * K = &state->hota_rc4_ks[0];

  U = ((uint16_t)bytes[0] << 8) | bytes[1];
  V = ((uint16_t)bytes[2] << 8) | bytes[3];

  #ifdef DEBUG_HOTA
  fprintf (stderr, "\nSB_RC ENC U: %04X; ENC V: %04X; PPI: %d;", U, V, ptype);
  #endif

  // for (int i = 0; i < 80; i++) //may can end at 40, or remove loop and hard set to 36
  {

    int i = 36; //hardset i for idx1 that seems to be a constant

    //Calculate Clear V from S stream bytes
    S = (K[(i+0) % 80] << 0) | (K[(i+1) % 80] << 8);
    Y = V ^ S;

    //invert parity row bits if needed
    if (ptype)
      Y = ~Y;

    for (int j = 0; j < 80; j++) //can be in range of 0 to 80, verified
    {

      //calculate Clear U from Q stream bytes
      Q = (K[(j+0) % 80] << 8) | (K[(j+1) % 80] << 0);
      Z = U ^ Q;

      //Y and Z match, proceed
      if (Y == Z)
      {
        //hamming encode on the 11-bit data portion of Clear U (Z) to see if it matches 5 lsb
        unsigned char hamming[16]; memset(hamming, 0, sizeof(hamming));
        unsigned char hammchk[16]; memset(hammchk, 0, sizeof(hammchk));
        for (int h = 0; h < 16; h++)
          hamming[h] = (Z >> (15 - h)) & 1;
        Hamming_16_11_4_encode(hamming, hammchk);

        //hamming result is positive, successful search
        if (memcmp(hamming, hammchk, 16) == 0)
        {
          #ifdef DEBUG_HOTA
          fprintf (stderr, "\nSuccess: IDX1: %d; IDX2: %d; ", i, j);
          fprintf (stderr, "S: %04X; Q: %04X; CLR U: %04X; CLR V: %04X;", S, Q, Z, Y);
          // fprintf (stderr, " HE: %04X; HC: %04X;", (uint16_t)convert_bits_into_output(hamming, 16), (uint16_t)convert_bits_into_output(hammchk, 16));
          #endif
          state->hota_sbrc_idx1[state->currentslot] = i;
          state->hota_sbrc_idx2[state->currentslot] = j;
          return;
        }
      }
    }
  }

}

uint8_t hytera_ep_data_decrypt(dsd_opts * opts, dsd_state * state, uint8_t * input, int len)
{
  uint8_t slot = state->currentslot;
  if (slot == 0 && state->payload_algid != 0xD0)
    return 0;
  else if (slot == 1 && state->payload_algidR != 0xD0)
    return 0;

  //TODO: Attempt Load by TG value as well
  //TODO: Hytera EP Data Header does have a Key ID,
  //attempt loading by either TG or Key ID as well

  unsigned long long int R = 0;
  if (slot == 0)
    R = state->R;
  else R = state->RR;

  if (R == 0)
    return 0;

  unsigned long long int mi = 0;
  if (slot == 0)
    mi = state->payload_mi;
  else mi = state->payload_miR;

  if (mi == 0)
    return 0;

  //TODO: 2000 -> expected size of decryption
  hytera_enhanced_rc4_setup(opts, state, R, mi, 2000);

  uint8_t ks_bytes[2000];
  memset(ks_bytes, 0, sizeof(ks_bytes));

  if (slot == 0)
    memcpy(ks_bytes, state->ks_octetL, 2000);
  else memcpy(ks_bytes, state->ks_octetR, 2000);

  //find current idx for keystream application
  int bp_idx = (state->data_block_counter[slot] - 1) * len;

  //apply keystream
  for (int i = 0; i < len; i++)
    input[i] ^= ks_bytes[bp_idx++];

  //return this is decrypted
  return 1;

}

uint8_t hytera_bp_data_decrypt(dsd_state * state, uint8_t * input, int len)
{

  uint8_t slot = state->currentslot;
  if (slot == 0 && state->payload_algid != 0x68)
    return 0;
  else if (slot == 1 && state->payload_algidR != 0x68)
    return 0;

  //load Hytera BP Key
  uint8_t key_loaded = 0;
  uint8_t key[32]; memset(key, 0, sizeof(key));
  uint8_t empt[32]; memset(empt, 0, sizeof(empt));

  //TODO: Attempt Load by TG value as well
  //TODO: Hytera BP Data Header does have a Key ID,
  //attempt loading by either TG or Key ID as well

  for (int i = 0; i < 8; i++)
  {
    key[i+0]   = (state->K1) >> (56-(i*8)) & 0xFF;
    key[i+8]   = (state->K2) >> (56-(i*8)) & 0xFF;
    key[i+16]  = (state->K3) >> (56-(i*8)) & 0xFF;
    key[i+24]  = (state->K4) >> (56-(i*8)) & 0xFF;
  }

  //if key is loaded with a key, then flag on the key loaded variable
  if (memcmp(key, empt, sizeof(key)) != 0) key_loaded = 1;

  if (key_loaded == 0)
    return 0;

  uint8_t key_bytes[64];
  memset (key_bytes, 0, sizeof(key_bytes));

  //Find len on loaded Hytera BP key w/ memcmp on key and load into key_bytes
  int key_len = 32;

  //TODO: We are assumming the key isn't something like 0x100000000000000000000000000000.... zero fill to 16 or 32
  //need to fix this checkdown, but may not be plausible without redoing the Hytera BP code entirely at this point

  //test for 64 char (32 byte) key
  if ((memcmp(empt, key+16, 8) != 0) || (memcmp(empt, key+24, 8) != 0))
  {
    memcpy(key_bytes, key, 32);
    key_len = 32;
  }
  //test for 32 char (16 byte) key
  else if ((memcmp(empt, key+8, 8) != 0))
  {
    memcpy(key_bytes, key, 16);
    key_len = 16;
  }
  //this is a 10 char (5 byte) key w/ offset
  else
  {
    memcpy(key_bytes, key+3, 5);
    key_len = 5;
  }

  //find current idx for keystream application
  int bp_idx = (state->data_block_counter[slot] - 1) * len;

  //apply keystream
  for (int i = 0; i < len; i++)
    input[i] ^= key_bytes[bp_idx++ % key_len];

  //return this is decrypted
  return 1;

}
