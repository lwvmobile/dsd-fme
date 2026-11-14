#include "dsd.h"
#include "pc4.h"

//function prototype linked in crypt-des.c
void ksun_des_cipher (uint8_t * key, uint8_t * input_register, uint8_t * output_register, uint8_t de);

/* === Global MD2II buffers === */
int x1, x2, i, n1global;
unsigned char h2[n1];
unsigned char h1[n1*3];

/* === MD2II functions === */
void MD2II_init()
{
    x1 = 0;
    x2 = 0;
    for (i = 0; i < n1global; i++)
        h2[i] = 0;
    for (i = 0; i < n1global; i++)
        h1[i] = 0;
}

void MD2II_hashing(unsigned char t1[], size_t b6)
{
    static unsigned char s4[256] =
    {13,199,11,67,237,193,164,77,115,184,141,222,73,
     38,147,36,150,87,21,104,12,61,156,101,111,145,
     119,22,207,35,198,37,171,167,80,30,219,28,213,
     121,86,29,214,242,6,4,89,162,110,175,19,157,
     3,88,234,94,144,118,159,239,100,17,182,173,238,
     68,16,79,132,54,163,52,9,58,57,55,229,192,
     170,226,56,231,187,158,70,224,233,245,26,47,32,
     44,247,8,251,20,197,185,109,153,204,218,93,178,
     212,137,84,174,24,120,130,149,72,180,181,208,255,
     189,152,18,143,176,60,249,27,227,128,139,243,253,
     59,123,172,108,211,96,138,10,215,42,225,40,81,
     65,90,25,98,126,154,64,124,116,122,5,1,168,
     83,190,131,191,244,240,235,177,155,228,125,66,43,
     201,248,220,129,188,230,62,75,71,78,34,31,216,
     254,136,91,114,106,46,217,196,92,151,209,133,51,
     236,33,252,127,179,69,7,183,105,146,97,39,15,
     205,112,200,166,223,45,48,246,186,41,148,140,107,
     76,85,95,194,142,50,49,134,23,135,169,221,210,
     203,63,165,82,161,202,53,14,206,232,103,102,195,
     117,250,99,0,74,160,241,2,113};

    int b1, b2, b3, b4 = 0, b5;

    while (b6) {
        for (; b6 && x2 < n1global; b6--, x2++) {
            b5 = t1[b4++];
            h1[x2 + n1global] = b5;
            h1[x2 + (n1global*2)] = b5 ^ h1[x2];
            x1 = h2[x2] ^= s4[b5 ^ x1];
        }

        if (x2 == n1global) {
            b2 = 0;
            x2 = 0;
            for (b3 = 0; b3 < (n1global+2); b3++) {
                for (b1 = 0; b1 < (n1global*3); b1++)
                    b2 = h1[b1] ^= s4[b2];
                b2 = (b2 + b3) % 256;
            }
        }
    }
}

void MD2II_end(unsigned char h4[n1])
{
    unsigned char h3[n1];
    int i, n4;
    n4 = n1global - x2;
    for (i = 0; i < n4; i++) h3[i] = n4;
    MD2II_hashing(h3, n4);
    MD2II_hashing(h2, n1global);
    for (i = 0; i < n1global; i++) h4[i] = h1[i];
}

void print_hex(const unsigned char *buf, int len)
{
    for (int i = 0; i < len; i++)
        printf("%02X ", buf[i]);
    printf("\n");
}

static inline uint64_t rol48(uint64_t x, int n)
{
    return ((x << n) | (x >> 47)) & 0xffffffffffff;
}

/* === Keystream (A5-like) === */
static int threshold(uint64_t r1, uint64_t r2, uint64_t r3)
{
    int total = (((r1 >> 31) & 1) == 1) +
                (((r2 >> 31) & 1) == 1) +
                (((r3 >> 31) & 1) == 1);
    return (total > 1) ? 0 : 1;
}

static uint64_t clock_r1(int ctl, uint64_t r1)
{
    ctl ^= ((r1 >> 31) & 1);
    if (ctl) {
        int taps[] = {0,3,5,9,10,11,12,17,18,28,33,34,35,36,37,39,42,43,44,46,47,49,50,57,60,61,62,63};
        uint64_t fb = 0;
        for(int i=0;i<sizeof(taps)/sizeof(int);i++) fb ^= (r1>>taps[i])&1ULL;
        r1 = (r1<<1)&0xFFFFFFFFFFFFFFFFULL;
        if(fb&1ULL) r1 ^= 1ULL;
    }
    return r1;
}

static uint64_t clock_r2(int ctl, uint64_t r2)
{
    ctl ^= ((r2 >> 31) & 1);
    if (ctl) {
        int taps[] = {0,3,5,8,9,10,12,13,15,17,19,20,21,22,24,27,30,31,33,34,35,36,37,40,41,42,51,52,55,56,59,60,62,63};
        uint64_t fb = 0;
        for(int i=0;i<sizeof(taps)/sizeof(int);i++) fb ^= (r2>>taps[i])&1ULL;
        r2 = (r2<<1)&0xFFFFFFFFFFFFFFFFULL;
        if(fb&1ULL) r2 ^= 1ULL;
    }
    return r2;
}

static uint64_t clock_r3(int ctl, uint64_t r3)
{
    ctl ^= ((r3 >> 31) & 1);
    if (ctl) {
        int taps[] = {1,2,4,5,6,7,8,9,10,14,15,16,17,18,22,23,25,26,27,28,29,31,32,34,35,36,38,41,42,43,44,45,47,48,49,50,51,54,55,59,61,63};
        uint64_t fb = 0;
        for(int i=0;i<sizeof(taps)/sizeof(int);i++) fb ^= (r3>>taps[i])&1ULL;
        r3 = (r3<<1)&0xFFFFFFFFFFFFFFFFULL;
        if(fb&1ULL) r3 ^= 1ULL;
    }
    return r3;
}

void keystream37(unsigned char *key, uint64_t frame, unsigned char *output)
{
    uint64_t r1=0,r2=0,r3=0;
    for(int i=0;i<8;i++) r1=(r1<<8)|key[i];
    for(int i=0;i<8;i++) r2=(r2<<8)|key[i+8];
    for(int i=0;i<8;i++) r3=(r3<<8)|key[i+16];

    for(int i=0;i<64;i++){
        int ctl=threshold(r1,r2,r3);
        r1=clock_r1(ctl,r1);
        r2=clock_r2(ctl,r2);
        r3=clock_r3(ctl,r3);
        if(frame&1ULL){ r1^=1ULL; r2^=1ULL; r3^=1ULL;}
        frame>>=1;
    }

    for(int i=0;i<384;i++){
        int ctl=threshold(r1,r2,r3);
        r1=clock_r1(ctl,r1);
        r2=clock_r2(ctl,r2);
        r3=clock_r3(ctl,r3);
    }

    unsigned char *ptr=output;
    unsigned char byte=0;
    int bits=0;
    for(int i=0;i<1008;i++){
        int ctl=threshold(r1,r2,r3);
        r1=clock_r1(ctl,r1);
        r2=clock_r2(ctl,r2);
        r3=clock_r3(ctl,r3);
        int bit=((r1>>63)^(r2>>63)^(r3>>63))&1U;
        byte=(byte<<1)|bit;
        bits++;
        if(bits==8){*ptr++=byte; bits=0; byte=0;}
    }
    if(bits) *ptr=byte;

}

#define ROTATE_LEFT(VALUE, SHIFT) \
    (((VALUE) << ((SHIFT) & 63)) | ((VALUE) >> (64 - ((SHIFT) & 63))))

#define ROTATE_RIGHT(VALUE, SHIFT) \
    (((VALUE) >> ((SHIFT) & 63)) | ((VALUE) << (64 - ((SHIFT) & 63))))

void keystream35(uint8_t * input_register, unsigned char * key)
{

  uint64_t round = 0x9E3779B97F4A7C15;
  uint64_t round_key[2]; memset(round_key, 0, sizeof(round_key));

  uint64_t state[2]; memset(state, 0, sizeof(state));

  //load input_register into state as two 64-bit chunks
  for (int i = 0; i < 8; i++)
  {
    state[0] <<= 8;
    state[0]  |= input_register[i+0];

    state[1] <<= 8;
    state[1]  |= input_register[i+8];
  }

  //load key into round_key as two 64-bit chunks
  for (int i = 0; i < 8; i++)
  {
    round_key[0] <<= 8;
    round_key[0]  |= key[i+0];

    round_key[1] <<= 8;
    round_key[1]  |= key[i+8];
  }

  for (int i = 0; i < 64; i++)
  {
    round_key[0] += round;
    round_key[1] -= round;

    state[0] ^= round_key[0];
    state[0] += state[1];
    state[0] = ROTATE_LEFT(state[0], state[1]);

    state[1] = ROTATE_RIGHT(state[1], state[0]);
    state[1] -= state[0];
    state[1] ^= round_key[1];
  }

  uint64_t temp = state[0];
  state[0] = state[1];
  state[1] = temp;

  //reload input_register with current state values
  for (int i = 0; i < 8; i++)
    input_register[i+0] = (state[0] >> (56 - (i * 8))) & 0xFF;

  for (int i = 0; i < 8; i++)
    input_register[i+8] = (state[1] >> (56 - (i * 8))) & 0xFF;

}

//dsd-fme -fs -Z -i 'SSD_STORAGE/2025_DEV/Tytera BP EP AP Master Folder/kirisun/cap-clear.wav/cap-33.wav' -H '3333333333333333 3333333333333333 3333333333333333 3333333333333333'
void kirisun_universal_keystream_creation(dsd_state *state)
{
  uint8_t slot = 0;
  if (state->currentslot == 0)
    slot = 0;
  else slot = 1;

  uint32_t lfsr = 0;
  if (state->currentslot == 0)
    lfsr = (uint32_t)state->payload_mi;
  else lfsr = (uint32_t)state->payload_miR;

  unsigned char userkeyhexa[32]; memset(userkeyhexa, 0, sizeof(userkeyhexa));
  unsigned char realkey[32]; memset(realkey, 0, sizeof(realkey));
  unsigned char lfsrhexa[4]; memset(lfsrhexa, 0, sizeof(lfsrhexa));
  uint8_t ks_bytes[126]; memset(ks_bytes, 0, sizeof(ks_bytes));

  lfsrhexa[0] = (lfsr >> 24) & 0xFF;
  lfsrhexa[1] = (lfsr >> 16) & 0xFF;
  lfsrhexa[2] = (lfsr >> 8)  & 0xFF;
  lfsrhexa[3] = (lfsr >> 0)  & 0xFF;

  uint64_t K1 = state->A1[slot];
  uint64_t K2 = state->A2[slot];
  uint64_t K3 = state->A3[slot];
  uint64_t K4 = state->A4[slot];

  if (K1 != 0 && K2 != 0 && K3 != 0 && K4 != 0)
    state->aes_key_loaded[slot] = 1;
  else state->aes_key_loaded[slot] = 0;

  if (state->aes_key_loaded[slot] == 1)
  {

    u64_to_bytes_be(K1, &userkeyhexa[0]);
    u64_to_bytes_be(K2, &userkeyhexa[8]);
    u64_to_bytes_be(K3, &userkeyhexa[16]);
    u64_to_bytes_be(K4, &userkeyhexa[24]);

    for(int i=0;i<32;i++) realkey[i]=userkeyhexa[i];

    /* --- MD2II first pass --- */
    n1global=32;
    unsigned char h4[n1]; memset(h4, 0, sizeof(h4));
    MD2II_init();
    MD2II_hashing(realkey,32);
    MD2II_end(h4);
    for(int i=0;i<32;i++) realkey[i]=h4[i];

    // printf("=== realkey final (32 bytes) ===\n");
    // print_hex(realkey,32);

    /* --- MD2II with n1global=8, hash lfsr byte by byte --- */
    unsigned char hashtag[1];

    n1global=8;
    MD2II_init();
    for(int i=0;i<4;i++)
    {
      hashtag[0] = (lfsr >> (24-8*i)) & 0xFF;
      MD2II_hashing(hashtag,1);
    }
    MD2II_hashing(realkey,32);
    MD2II_end(h4);

    // printf("h4[0..7]: ");
    // print_hex(h4,8);

    uint64_t internal_state=0;
    for(int i=0;i<8;i++)
      internal_state=(internal_state<<8)+(h4[i]);

    // printf("internal_state=%016llX\n",(unsigned long long)internal_state);

    /* --- MD2II with n1global=24 --- */
    n1global=24;
    MD2II_init();
    for(int i=0;i<4;i++)
    {
      hashtag[0] = (lfsr >> (24-8*i)) & 0xFF;
      MD2II_hashing(hashtag,1);
    }
    MD2II_hashing(realkey,32);
    MD2II_end(h4);

    // printf("realkey (24 bytes):\n");
    // print_hex(h4,24);

    /* --- keystream output --- */
    unsigned char output[126]; memset(output, 0, sizeof(output));
    keystream37(h4, internal_state, output);

    // printf("keystream output (126 bytes):\n");
    // print_hex(output,126);

    //store ks_bytes to storage
    for (int i = 0; i < 126; i++)
      ks_bytes[i] = (uint8_t)output[i];

    if (slot == 0)
      memcpy(state->ks_octetL, ks_bytes, sizeof(ks_bytes));
    else memcpy(state->ks_octetR, ks_bytes, sizeof(ks_bytes));

    //debug ks_bytes
    // fprintf (stderr, "KS: ");
    // for (int i = 0; i < 126; i++)
    // {
    //   if ((i != 0) && ((i%16)==0))
    //     fprintf (stderr, "\n    ");
    //   fprintf (stderr, "%02X ", ks_bytes[i]);
    // }
    // fprintf (stderr, "\n");

  }

}

//dsd-fme -fs -Z -i 'SSD_STORAGE/2025_DEV/Tytera BP EP AP Master Folder/kirisun/better_samples_tg1/kirisun_advanced_tg1.wav' -H 'DC1A7E9F9BF312DB F45010CEC5F7A53A C407D0BFA803617B E426A7254DA9390D'
void kirisun_advanced_keystream_creation(dsd_state *state)
{

  uint8_t slot = 0;
  if (state->currentslot == 0)
    slot = 0;
  else slot = 1;

  uint32_t lfsr = 0;
  if (state->currentslot == 0)
    lfsr = (uint32_t)state->payload_mi;
  else lfsr = (uint32_t)state->payload_miR;
  
  unsigned char userkeyhexa[32]; memset(userkeyhexa, 0, sizeof(userkeyhexa));
  unsigned char realkey[32]; memset(realkey, 0, sizeof(realkey));
  unsigned char lfsrhexa[4]; memset(lfsrhexa, 0, sizeof(lfsrhexa));
  uint8_t ks_bytes[126]; memset(ks_bytes, 0, sizeof(ks_bytes));

  lfsrhexa[0] = (lfsr >> 24) & 0xFF;
  lfsrhexa[1] = (lfsr >> 16) & 0xFF;
  lfsrhexa[2] = (lfsr >> 8)  & 0xFF;
  lfsrhexa[3] = (lfsr >> 0)  & 0xFF;

  uint64_t K1 = state->A1[slot];
  uint64_t K2 = state->A2[slot];
  uint64_t K3 = state->A3[slot];
  uint64_t K4 = state->A4[slot];

  if (K1 != 0 && K2 != 0 && K3 != 0 && K4 != 0)
    state->aes_key_loaded[slot] = 1;
  else state->aes_key_loaded[slot] = 0;

  if (state->aes_key_loaded[slot] == 1)
  {

    u64_to_bytes_be(K1, &userkeyhexa[0]);
    u64_to_bytes_be(K2, &userkeyhexa[8]);
    u64_to_bytes_be(K3, &userkeyhexa[16]);
    u64_to_bytes_be(K4, &userkeyhexa[24]);

    //init vital portions of PC4Context
    ctx.x1 = 0;
    ctx.x2 = 0;
    for (ctx.i = 0; ctx.i < n1; ctx.i++) ctx.h2[ctx.i] = 0;
    for (ctx.i = 0; ctx.i < n1; ctx.i++) ctx.h1[ctx.i] = 0;
    ctx.rounds = nbround;

    //create keys
    create_keys(&ctx, userkeyhexa, 32);

    /* --- MD2II first pass --- */
    n1global=32;
    unsigned char h4[n1]; memset(h4, 0, sizeof(h4));
    MD2II_init();
    MD2II_hashing(userkeyhexa,32);
    MD2II_end(h4);
    for(int i=0;i<32;i++) realkey[i]=h4[i];

    // printf("=== realkey final (32 bytes) ===\n");
    // print_hex(realkey,32);

    n1global=32;
    memset(h4, 0, sizeof(h4));
    MD2II_init();
    MD2II_hashing(lfsrhexa,4);
    MD2II_hashing(realkey,32);
    MD2II_end(h4);

    // printf("h4[0..7]: ");
    // print_hex(h4,8);

    uint64_t internal_state=0;
    for(int i=0;i<6;i++)
      internal_state=(internal_state<<8)+(h4[i]);
    
    int k = 0;
    for (int i = 0; i < 18; i++)
    {

      //debug internal_state
      // fprintf (stderr, "ST: %012lX; \n", internal_state);

      ctx.convert[0]=(internal_state>>40)&0xff;
      ctx.convert[1]=(internal_state>>32)&0xff;
      ctx.convert[2]=(internal_state>>24)&0xff;
      ctx.convert[3]=(internal_state>>16)&0xff;
      ctx.convert[4]=(internal_state>>8)&0xff;
      ctx.convert[5]=internal_state&0xff;

      pc4encrypt(&ctx);

      internal_state=0;
      for (int i=0;i<6;i++)
        internal_state=(internal_state<<8)+(ctx.convert[i]&0xff);

      internal_state=rol48(internal_state,1);

      //keystream
      for (int i=0;i<6;i++)
        ks_bytes[k++] = ctx.convert[i];
      k++; //skip 7th octet

    }

    //store ks_bytes to storage
    if (slot == 0)
      memcpy(state->ks_octetL, ks_bytes, sizeof(ks_bytes));
    else memcpy(state->ks_octetR, ks_bytes, sizeof(ks_bytes));

    //debug ks_bytes
    // fprintf (stderr, "KS: ");
    // for (int i = 0; i < 126; i++)
    // {
    //   if ((i != 0) && ((i%16)==0))
    //     fprintf (stderr, "\n    ");
    //   fprintf (stderr, "%02X ", ks_bytes[i]);
    // }
    // fprintf (stderr, "\n");

  }

}

//dsd-fme -fs -Z -i 'SSD_STORAGE/2025_DEV/Tytera BP EP AP Master Folder/kirisun/enhanced-key-12.wav/enhanced-key-12.wav' -H 'C607727B8BECC3CB 7DD174F5526BB546'
void kirisun_enhanced_keystream_creation(dsd_state *state)
{
  uint8_t slot = 0;
  if (state->currentslot == 0)
    slot = 0;
  else slot = 1;

  uint32_t lfsr = 0;
  if (state->currentslot == 0)
    lfsr = (uint32_t)state->payload_mi;
  else lfsr = (uint32_t)state->payload_miR;

  unsigned char userkeyhexa[32]; memset(userkeyhexa, 0, sizeof(userkeyhexa));
  unsigned char realkey[32]; memset(realkey, 0, sizeof(realkey));
  unsigned char lfsrhexa[4]; memset(lfsrhexa, 0, sizeof(lfsrhexa));
  uint8_t ks_bytes[128]; memset(ks_bytes, 0, sizeof(ks_bytes));

  lfsrhexa[0] = (lfsr >> 24) & 0xFF;
  lfsrhexa[1] = (lfsr >> 16) & 0xFF;
  lfsrhexa[2] = (lfsr >> 8)  & 0xFF;
  lfsrhexa[3] = (lfsr >> 0)  & 0xFF;

  uint64_t K1 = state->A1[slot];
  uint64_t K2 = state->A2[slot];

  if (K1 != 0 && K2 != 0)
    state->aes_key_loaded[slot] = 1;
  else state->aes_key_loaded[slot] = 0;

  if (state->aes_key_loaded[slot] == 1)
  {

    u64_to_bytes_be(K1, &userkeyhexa[0]);
    u64_to_bytes_be(K2, &userkeyhexa[8]);

    // printf("=== userkey input (16 bytes) ===\n");
    // print_hex(userkeyhexa,16);

    /* --- MD2II first pass --- */
    n1global=16;
    unsigned char h4[n1]; memset(h4, 0, sizeof(h4));
    MD2II_init();
    MD2II_hashing(userkeyhexa,16);
    MD2II_end(h4);
    for(int i=0;i<16;i++) realkey[i]=h4[i];

    // printf("=== realkey final (16 bytes) ===\n");
    // print_hex(realkey,16);

    n1global=16;
    MD2II_init();
    MD2II_hashing(lfsrhexa,4);
    MD2II_hashing(realkey,16);
    MD2II_end(h4);

    uint8_t input_register[16]; memset(input_register, 0, sizeof(input_register));

    //copy h4 to the input_register for the cipher
    memcpy(input_register, h4, 16);

    /* --- MD2II with n1global=32 --- */
    n1global=32;
    MD2II_init();
    MD2II_hashing(lfsrhexa,4);
    MD2II_hashing(realkey,16);
    MD2II_end(h4);

    //copy h4 to the tab for the cipher
    uint8_t tab[16]; memset(tab, 0, sizeof(tab));
    memcpy(tab, h4, 16);

    // printf("=== Input Register (16 bytes) ===\n");
    // print_hex(input_register,16);

    // printf("=== Tab (16 bytes) ===\n");
    // print_hex(tab,16);

    /* --- keystream output --- */
    uint8_t output[128]; memset(output, 0, sizeof(output));

    //discard round
    keystream35(input_register, tab);

    for (int i = 0; i < 8; i++)
    {
      keystream35(input_register, tab);
      memcpy(output+(i*16), input_register, 16);
    }

    //store ks_bytes to storage
    for (int i = 0; i < 128; i++)
      ks_bytes[i] = output[i];

    if (slot == 0)
      memcpy(state->ks_octetL, ks_bytes, sizeof(ks_bytes));
    else memcpy(state->ks_octetR, ks_bytes, sizeof(ks_bytes));

    //debug ks_bytes
    // fprintf (stderr, "KS: ");
    // for (int i = 0; i < 128; i++)
    // {
    //   if ((i != 0) && ((i%16)==0))
    //     fprintf (stderr, "\n    ");
    //   fprintf (stderr, "%02X ", ks_bytes[i]);
    // }
    // fprintf (stderr, "\n");

  }

}

//dsd-fme -fs -Z -i 'SSD_STORAGE/2025_DEV/Tytera BP EP AP Master Folder/kirisun/kirisun_files/reinforced-key-9.wav' -H 'C607727B8BECC3CB 7DD174F5526BB546 B3DEA1C111DC1994 69243120CDB6F99A'
void kirisun_reinforced_keystream_creation(dsd_state *state)
{
  uint8_t slot = 0;
  if (state->currentslot == 0)
    slot = 0;
  else slot = 1;

  uint32_t lfsr = 0;
  if (state->currentslot == 0)
    lfsr = (uint32_t)state->payload_mi;
  else lfsr = (uint32_t)state->payload_miR;

  unsigned char userkeyhexa[32]; memset(userkeyhexa, 0, sizeof(userkeyhexa));
  unsigned char realkey[32]; memset(realkey, 0, sizeof(realkey));
  unsigned char lfsrhexa[4]; memset(lfsrhexa, 0, sizeof(lfsrhexa));
  uint8_t ks_bytes[128]; memset(ks_bytes, 0, sizeof(ks_bytes));

  lfsrhexa[0] = (lfsr >> 24) & 0xFF;
  lfsrhexa[1] = (lfsr >> 16) & 0xFF;
  lfsrhexa[2] = (lfsr >> 8)  & 0xFF;
  lfsrhexa[3] = (lfsr >> 0)  & 0xFF;

  uint64_t K1 = state->A1[slot];
  uint64_t K2 = state->A2[slot];
  uint64_t K3 = state->A3[slot];
  uint64_t K4 = state->A4[slot];

  if (K1 != 0 && K2 != 0 && K3 != 0 && K4 != 0)
    state->aes_key_loaded[slot] = 1;
  else state->aes_key_loaded[slot] = 0;

  if (state->aes_key_loaded[slot] == 1)
  {

    u64_to_bytes_be(K1, &userkeyhexa[0]);
    u64_to_bytes_be(K2, &userkeyhexa[8]);
    u64_to_bytes_be(K3, &userkeyhexa[16]);
    u64_to_bytes_be(K4, &userkeyhexa[24]);

    for(int i=0;i<32;i++) realkey[i]=userkeyhexa[i];

    // printf("=== userkey input (32 bytes) ===\n");
    // print_hex(userkeyhexa,32);

    /* --- MD2II first pass --- */
    n1global=32;
    unsigned char h4[n1]; memset(h4, 0, sizeof(h4));
    MD2II_init();
    MD2II_hashing(realkey,32);
    MD2II_end(h4);
    for(int i=0;i<32;i++) realkey[i]=h4[i];

    // printf("=== realkey final (32 bytes) ===\n");
    // print_hex(realkey,32);

    n1global=8;
    MD2II_init();
    MD2II_hashing(lfsrhexa,4);
    MD2II_hashing(realkey,32);
    MD2II_end(h4);

    // printf("h4[0..7]: ");
    // print_hex(h4,8);

    uint64_t internal_state=0;
    for(int i=0;i<8;i++)
      internal_state=(internal_state<<8)+(h4[i]);

    // printf("internal_state=%016llX\n",(unsigned long long)internal_state);

    uint8_t input_register[8]; memset(input_register, 0, sizeof(input_register));
    uint8_t output_register[8]; memset(output_register, 0, sizeof(output_register));

    //copy internal state value to the input_register for the modified DES cipher
    memcpy(input_register, h4, 8);

    /* --- MD2II with n1global=96 --- */
    n1global=96;
    MD2II_init();
    MD2II_hashing(lfsrhexa,4);
    MD2II_hashing(realkey,32);
    MD2II_end(h4);

    // printf("realkey (96 bytes):\n");
    // print_hex(h4,96);

    /* --- keystream output --- */
    uint8_t output[128]; memset(output, 0, sizeof(output));

    //run discard round first
    ksun_des_cipher(h4, input_register, output_register, 1);
    memcpy(input_register, output_register, 8); //internal_state = des_reinforced(internal_state, encrypt);
    memset(output_register, 0, sizeof(output_register));

    //begin creation of actual keystream now
    for (int i = 0; i < 16; i++)
    {
      //the internal state is the output feedback mode
      ksun_des_cipher(h4, input_register, output_register, 1);
      memcpy(output+(i*8), output_register, 8);
      memcpy(input_register, output_register, 8);
      memset(output_register, 0, sizeof(output_register));
    }

    // printf("keystream output (128 bytes):\n");
    // print_hex(output,128);

    //store ks_bytes to storage
    for (int i = 0; i < 128; i++)
      ks_bytes[i] = output[i];

    if (slot == 0)
      memcpy(state->ks_octetL, ks_bytes, sizeof(ks_bytes));
    else memcpy(state->ks_octetR, ks_bytes, sizeof(ks_bytes));

    //debug ks_bytes
    // fprintf (stderr, "KS: ");
    // for (int i = 0; i < 128; i++)
    // {
    //   if ((i != 0) && ((i%16)==0))
    //     fprintf (stderr, "\n    ");
    //   fprintf (stderr, "%02X ", ks_bytes[i]);
    // }
    // fprintf (stderr, "\n");

  }

}

//dsd-fme -fs -Z -i 'SSD_STORAGE/2025_DEV/Tytera BP EP AP Master Folder/kirisun/kirisun_files/strengthened-key-7.wav' -H 'C607727B8BECC3CB 7DD174F5526BB546 B3DEA1C111DC1994 69243120CDB6F99A'
void kirisun_strengthened_keystream_creation(dsd_state *state)
{
  uint8_t slot = 0;
  if (state->currentslot == 0)
    slot = 0;
  else slot = 1;

  uint32_t lfsr = 0;
  if (state->currentslot == 0)
    lfsr = (uint32_t)state->payload_mi;
  else lfsr = (uint32_t)state->payload_miR;

  unsigned char userkeyhexa[32]; memset(userkeyhexa, 0, sizeof(userkeyhexa));
  unsigned char realkey[32]; memset(realkey, 0, sizeof(realkey));
  unsigned char lfsrhexa[4]; memset(lfsrhexa, 0, sizeof(lfsrhexa));
  uint8_t ks_bytes[128]; memset(ks_bytes, 0, sizeof(ks_bytes));
  uint8_t global_key[288]; memset (global_key, 0, sizeof(global_key));

  lfsrhexa[0] = (lfsr >> 24) & 0xFF;
  lfsrhexa[1] = (lfsr >> 16) & 0xFF;
  lfsrhexa[2] = (lfsr >> 8)  & 0xFF;
  lfsrhexa[3] = (lfsr >> 0)  & 0xFF;

  uint64_t K1 = state->A1[slot];
  uint64_t K2 = state->A2[slot];
  uint64_t K3 = state->A3[slot];
  uint64_t K4 = state->A4[slot];

  if (K1 != 0 && K2 != 0 && K3 != 0 && K4 != 0)
    state->aes_key_loaded[slot] = 1;
  else state->aes_key_loaded[slot] = 0;

  if (state->aes_key_loaded[slot] == 1)
  {

    u64_to_bytes_be(K1, &userkeyhexa[0]);
    u64_to_bytes_be(K2, &userkeyhexa[8]);
    u64_to_bytes_be(K3, &userkeyhexa[16]);
    u64_to_bytes_be(K4, &userkeyhexa[24]);

    for(int i=0;i<32;i++) realkey[i]=userkeyhexa[i];

    // printf("=== userkey input (32 bytes) ===\n");
    // print_hex(userkeyhexa,32);

    /* --- MD2II first pass --- */
    n1global=32;
    unsigned char h4[288]; memset(h4, 0, sizeof(h4));
    MD2II_init();
    MD2II_hashing(realkey,32);
    MD2II_end(h4);
    for(int i=0;i<32;i++) realkey[i]=h4[i];

    // printf("=== realkey final (32 bytes) ===\n");
    // print_hex(realkey,32);

    n1global=288;
    MD2II_init();
    MD2II_hashing(realkey, 32);
    MD2II_end(h4);
    for (int i=0;i<288;i++) global_key[i]=h4[i];

    // printf("=== global key (288 bytes) ===\n");
    // print_hex(realkey,288);

    n1global=8;
    MD2II_init();
    MD2II_hashing(lfsrhexa,4);
    MD2II_hashing(realkey,32);
    MD2II_end(h4);

    // printf("h4[0..7]: ");
    // print_hex(h4,8);

    uint64_t internal_state=0;
    for(int i=0;i<8;i++)
      internal_state=(internal_state<<8)+(h4[i]);

    // printf("internal_state=%016llX\n",(unsigned long long)internal_state);

    uint8_t input_register[8]; memset(input_register, 0, sizeof(input_register));
    uint8_t output_register[8]; memset(output_register, 0, sizeof(output_register));

    //copy internal state value to the input_register for the modified DES cipher
    memcpy(input_register, h4, 8);

    /* --- MD2II with n1global=96 --- */
    n1global=96;
    MD2II_init();
    MD2II_hashing(lfsrhexa,4);
    MD2II_hashing(realkey,32);
    MD2II_end(h4);

    // printf("realkey (96 bytes):\n");
    // print_hex(h4,96);

    /* --- keystream output --- */
    uint8_t output[128]; memset(output, 0, sizeof(output));

    //run discard round first
    ksun_des_cipher(global_key+0, input_register, output_register, 1);
    memcpy(input_register, output_register, 8); //internal_state = des_reinforced(internal_state, encrypt);
    memset(output_register, 0, sizeof(output_register));

    ksun_des_cipher(global_key+96, input_register, output_register, 0);
    memcpy(input_register, output_register, 8); //internal_state = des_reinforced(internal_state, encrypt);
    memset(output_register, 0, sizeof(output_register));

    ksun_des_cipher(global_key+192, input_register, output_register, 1);
    memcpy(input_register, output_register, 8); //internal_state = des_reinforced(internal_state, encrypt);
    memset(output_register, 0, sizeof(output_register));

    //begin creation of actual keystream now
    for (int i = 0; i < 16; i++)
    {
      //the internal state is the output feedback mode
      ksun_des_cipher(global_key+0, input_register, output_register, 1);
      memcpy(input_register, output_register, 8); //internal_state = des_reinforced(internal_state, encrypt);
      memset(output_register, 0, sizeof(output_register));

      ksun_des_cipher(global_key+96, input_register, output_register, 0);
      memcpy(input_register, output_register, 8); //internal_state = des_reinforced(internal_state, encrypt);
      memset(output_register, 0, sizeof(output_register));

      ksun_des_cipher(global_key+192, input_register, output_register, 1);
      memcpy(input_register, output_register, 8); //internal_state = des_reinforced(internal_state, encrypt);

      //copy out last output_register to output
      memcpy(output+(i*8), output_register, 8);
      memset(output_register, 0, sizeof(output_register));     
    }

    // printf("keystream output (128 bytes):\n");
    // print_hex(output,128);

    //store ks_bytes to storage
    for (int i = 0; i < 128; i++)
      ks_bytes[i] = output[i];

    if (slot == 0)
      memcpy(state->ks_octetL, ks_bytes, sizeof(ks_bytes));
    else memcpy(state->ks_octetR, ks_bytes, sizeof(ks_bytes));

    //debug ks_bytes
    // fprintf (stderr, "KS: ");
    // for (int i = 0; i < 128; i++)
    // {
    //   if ((i != 0) && ((i%16)==0))
    //     fprintf (stderr, "\n    ");
    //   fprintf (stderr, "%02X ", ks_bytes[i]);
    // }
    // fprintf (stderr, "\n");

  }
}

void kirisun_low_keystream_creation(dsd_state *state)
{
  UNUSED(state);
}

void kirisun_middle_keystream_creation(dsd_state *state)
{
  UNUSED(state);
}

void kirisun_high_keystream_creation(dsd_state *state)
{
  UNUSED(state);
}

//start alg 39 "top"
#define NUM_REGS_MAIN 7
#define NUM_REGS_CTRL 3
#define NUM_REGS (NUM_REGS_MAIN + NUM_REGS_CTRL)
#define TOTAL_MASKS 64
#define KEY_BITS 640
#define FRAME_BITS 64

typedef struct {
    uint64_t regs[NUM_REGS];
    uint64_t masks[NUM_REGS];
    uint8_t out_bit_pos[NUM_REGS];
    uint8_t maj_bit_pos_ctrl[NUM_REGS_CTRL];
    uint32_t global_out_counter;
    uint8_t global_choice;
} CipherState39;

static const uint64_t ALL_MASKS[TOTAL_MASKS] = {
    0xDAD4A9CEF35E2420ULL, 0xDCDD39248075249BULL, 0x96858C3E324E3D7FULL,
    0xE4D275E07AF08B7CULL, 0xB507D951563F5C36ULL, 0xB2780872314F0F86ULL,
    0xD519B8BB4C481CE4ULL, 0x842A1B95E05E7F87ULL, 0x91E9439B2886B3AAULL,
    0xA7122188120A212FULL, 0xAE4D0BF20D7ADB6BULL, 0x83C7BF84107C2891ULL,
    0xAABAC2ADCE754BBAULL, 0xD5B05083F5D472D0ULL, 0xC15B68FE3DAF1D91ULL,
    0xA26EAAFC534E79E6ULL, 0xAB1CF8D68E5B3A97ULL, 0xC9C43FF0E94D1580ULL,
    0x8DE7098DFF722919ULL, 0xDC528FBFAF59524FULL, 0x8C62BF3A202B869DULL,
    0x8B2AE4B656A42B07ULL, 0xE99E20DED6992F58ULL, 0x9D62A2B318CEB47FULL,
    0x97B23916A28CE239ULL, 0xE3AEDDC607816193ULL, 0xEC431D8F1667ED8AULL,
    0xC851867E91F4F41EULL, 0xCEA287E8004453DDULL, 0xF64FC71FC4D15B04ULL,
    0xB72E37386904EFEDULL, 0xACCF01D1058FECBFULL, 0xCFA36D08B25B05DFULL,
    0xC16281EC3B74C6D8ULL, 0x9CC8A285390AB24FULL, 0xA2CA5B1DDCFF5106ULL,
    0x9D4E71CA21F9D0F9ULL, 0xC3119E1CCE07FC95ULL, 0xB7BED36180EA8604ULL,
    0xE54926C4B8238E52ULL, 0x89CAD2795C19068AULL, 0xBA89993381C79575ULL,
    0xA0694F38A34560D9ULL, 0xA507C1D4A6D1AF9CULL, 0xA0A88043C051368DULL,
    0xBE451FBF6A22BDCEULL, 0xEC2040AE8197B977ULL, 0x8C5D783B907212D5ULL,
    0xBDE2462A14FD277CULL, 0xE017D20046CC81A3ULL, 0x940D57A83F499DF0ULL,
    0xDA74E12B0C248D67ULL, 0xF7485B9A998217E1ULL, 0x9A932895ECC7B023ULL,
    0xBE899F6138395F6DULL, 0x9845A2782ED4D1F1ULL, 0x97631234A75C11D5ULL,
    0x9ED48E5309D42CE1ULL, 0xD2DA8B5E23C96F07ULL, 0x9CF8B433FBC06E98ULL,
    0xF7FD22E242EEB923ULL, 0xBFD240478BDFA02FULL, 0xC2E6377C20F6CB4DULL, 0xBD313202C33EAF60ULL
};
static inline uint8_t get_bit(uint64_t r, uint8_t pos) { return (r >> pos) & 1U; }
static inline uint64_t clock_register(uint64_t r, uint64_t mask) {
    uint64_t lsb = r & 1ULL;
    r >>= 1;
    if (lsb) r ^= mask;
    return r;
}


static CipherState39 state39;

static uint8_t clock_ctrl_regs_a51(uint64_t regs[NUM_REGS_CTRL],
                                   uint64_t masks_ctrl[NUM_REGS_CTRL],
                                   uint8_t maj_bit_pos_ctrl[NUM_REGS_CTRL]);
static uint8_t clock_main_regs_modified(uint64_t regs_main[NUM_REGS_MAIN],
                                        uint64_t masks_main[NUM_REGS_MAIN],
                                        uint64_t regs_ctrl[NUM_REGS_CTRL],
                                        uint64_t masks_ctrl[NUM_REGS_CTRL],
                                        uint8_t out_bit_pos_ctrl[NUM_REGS_CTRL],
                                        uint8_t maj_bit_pos_ctrl[NUM_REGS_CTRL]);
static void select_10_masks_md2(uint8_t key80[80],
                                uint64_t masks[NUM_REGS],
                                uint8_t out_bit_pos[NUM_REGS],
                                uint8_t maj_bit_pos_ctrl[NUM_REGS_CTRL]) {



    unsigned char h4[264];
    unsigned char hash_bytes[32];
    n1global=32;
    MD2II_init();
    MD2II_hashing(key80, 80);
    MD2II_end(h4);
    for (int i=0;i<32;i++) hash_bytes[i]=h4[i];

    int mask_nums[NUM_REGS];
    for (int i = 0; i < NUM_REGS; i++) mask_nums[i] = -1;
    int count = 0, hash_index = 0;
    while (count < NUM_REGS) {
        uint8_t byte = hash_bytes[hash_index++];
        int idx = byte % TOTAL_MASKS;
        int unique = 1;
        for (int i = 0; i < count; i++) if (mask_nums[i] == idx) { unique = 0; break; }
        if (unique) {
            mask_nums[count] = idx;
            masks[count] = ALL_MASKS[idx];
            count++;
        }
        if (hash_index == 32) {

        MD2II_init();
        MD2II_hashing(hash_bytes, 32);
        MD2II_end(h4);
        for (int i=0;i<32;i++) hash_bytes[i]=h4[i];
        hash_index = 0;
        }
    }

    for (int i = 0; i < NUM_REGS; i++) {
        if (hash_index == 32) {
        MD2II_init();
        MD2II_hashing(hash_bytes, 32);
        MD2II_end(h4);
        for (int i=0;i<32;i++) hash_bytes[i]=h4[i];
        hash_index = 0;
        }
        uint8_t b = hash_bytes[hash_index++];
        uint8_t pos = (b % 63) + 1;
        out_bit_pos[i] = pos;
    }
    for (int i = 0; i < NUM_REGS_CTRL; i++) {
        if (hash_index == 32) {
        MD2II_init();
        MD2II_hashing(hash_bytes, 32);
        MD2II_end(h4);
        for (int i=0;i<32;i++) hash_bytes[i]=h4[i];
        hash_index = 0;
            hash_index = 0;
        }
        uint8_t b = hash_bytes[hash_index++];
        uint8_t pos = (b % 63) + 1;
        if (pos == out_bit_pos[NUM_REGS_MAIN + i]) {
            pos = (pos % 63) + 1;
        }
        maj_bit_pos_ctrl[i] = pos;
    }

    if (hash_index == 32) {
        MD2II_init();
        MD2II_hashing(hash_bytes, 32);
        MD2II_end(h4);
        for (int i=0;i<32;i++) hash_bytes[i]=h4[i];
        hash_index = 0;
    }
    uint8_t rnd_byte = hash_bytes[hash_index++];
    state39.global_choice = (uint8_t)((rnd_byte % 16) + 16);
    state39.global_out_counter = 0;
}
static uint8_t clock_ctrl_regs_a51(uint64_t regs[NUM_REGS_CTRL],
                                   uint64_t masks_ctrl[NUM_REGS_CTRL],
                                   uint8_t maj_bit_pos_ctrl[NUM_REGS_CTRL]) {
    uint8_t t0 = get_bit(regs[0], maj_bit_pos_ctrl[0]);
    uint8_t t1 = get_bit(regs[1], maj_bit_pos_ctrl[1]);
    uint8_t t2 = get_bit(regs[2], maj_bit_pos_ctrl[2]);
    uint8_t maj = (t0 + t1 + t2 >= 2) ? 1 : 0;
    uint8_t mask = 0;
    if (t0 == maj) { regs[0] = clock_register(regs[0], masks_ctrl[0]); mask |= 0x4; }
    if (t1 == maj) { regs[1] = clock_register(regs[1], masks_ctrl[1]); mask |= 0x2; }
    if (t2 == maj) { regs[2] = clock_register(regs[2], masks_ctrl[2]); mask |= 0x1; }
    return mask;
}
static uint8_t clock_main_regs_modified(uint64_t regs_main[NUM_REGS_MAIN],
                                        uint64_t masks_main[NUM_REGS_MAIN],
                                        uint64_t regs_ctrl[NUM_REGS_CTRL],
                                        uint64_t masks_ctrl[NUM_REGS_CTRL],
                                        uint8_t out_bit_pos_ctrl[NUM_REGS_CTRL],
                                        uint8_t maj_bit_pos_ctrl[NUM_REGS_CTRL]) {
    uint8_t mask = 0;
    uint8_t selector = 0;
    do {
        selector = 0;
        for (int k = 0; k < 7; k++) {
            uint8_t x = get_bit(regs_ctrl[0], out_bit_pos_ctrl[0])
                      ^ get_bit(regs_ctrl[1], out_bit_pos_ctrl[1])
                      ^ get_bit(regs_ctrl[2], out_bit_pos_ctrl[2]);
            selector = (uint8_t)((selector << 1) | (x & 1));
            (void)clock_ctrl_regs_a51(regs_ctrl, masks_ctrl, maj_bit_pos_ctrl);
        }
        selector &= 0x7F;
    } while (selector == 0);
    for (int i = 0; i < NUM_REGS_MAIN; i++) {
        if (selector & (1u << i)) {
            regs_main[i] = clock_register(regs_main[i], masks_main[i]);
            mask |= (1u << i);
        }
    }
    return mask;
}
static void apply_selector2_and_update_choice(uint64_t regs[NUM_REGS],
                                              uint64_t masks[NUM_REGS],
                                              uint8_t out_bit_pos[NUM_REGS],
                                              uint8_t maj_bit_pos_ctrl[NUM_REGS_CTRL]) {
    uint64_t *regs_main = &regs[0];
    uint64_t *regs_ctrl = &regs[NUM_REGS_MAIN];
    uint64_t *masks_main = &masks[0];
    uint64_t *masks_ctrl = &masks[NUM_REGS_MAIN];
    uint8_t selector2 = 0;
    for (int k = 0; k < NUM_REGS_MAIN; k++) {
        uint8_t c0 = get_bit(regs_ctrl[0], out_bit_pos[NUM_REGS_MAIN + 0]);
        uint8_t c1 = get_bit(regs_ctrl[1], out_bit_pos[NUM_REGS_MAIN + 1]);
        uint8_t c2 = get_bit(regs_ctrl[2], out_bit_pos[NUM_REGS_MAIN + 2]);
        uint8_t x = c0 ^ c1 ^ c2;
        selector2 = (uint8_t)((selector2 << 1) | (x & 1));
        (void)clock_ctrl_regs_a51(regs_ctrl, masks_ctrl, maj_bit_pos_ctrl);
    }
    if (selector2 != 0) {
        for (int i = 0; i < NUM_REGS_MAIN; i++) {
            if (selector2 & (1u << i)) {
                regs_main[i] = clock_register(regs_main[i], masks_main[i]);
            }
        }
    }
    uint8_t bits4 = 0;
    for (int k = 0; k < 4; k++) {
        uint8_t c0 = get_bit(regs_ctrl[0], out_bit_pos[NUM_REGS_MAIN + 0]);
        uint8_t c1 = get_bit(regs_ctrl[1], out_bit_pos[NUM_REGS_MAIN + 1]);
        uint8_t c2 = get_bit(regs_ctrl[2], out_bit_pos[NUM_REGS_MAIN + 2]);
        uint8_t x = c0 ^ c1 ^ c2;
        bits4 = (uint8_t)((bits4 << 1) | (x & 1));
        (void)clock_ctrl_regs_a51(regs_ctrl, masks_ctrl, maj_bit_pos_ctrl);
    }
    state39.global_choice = (uint8_t)((bits4 % 16) + 16);
    state39.global_out_counter = 0;
}
static void init_regs(uint64_t regs[NUM_REGS],
                      uint64_t masks[NUM_REGS],
                      uint8_t key80[80],
                      uint64_t frame,
                      uint8_t out_bit_pos[NUM_REGS],
                      uint8_t maj_bit_pos_ctrl[NUM_REGS_CTRL]) {
    for (int i = 0; i < NUM_REGS; i++) {
        regs[i] = 0;
        for (int j = 0; j < 8; j++) if (i*8+j < 80) regs[i] |= ((uint64_t)key80[i*8+j]) << (j*8);
    }
    for (int i = 0; i < KEY_BITS; i++) {
        uint8_t kbit = (key80[i/8] >> (i%8)) & 1U;
        for (int j = 0; j < NUM_REGS; j++) regs[j] ^= kbit;
        for (int j = 0; j < NUM_REGS; j++) regs[j] = clock_register(regs[j], masks[j]);
    }
    for (int i = 0; i < FRAME_BITS; i++) {
        uint8_t fbit = (frame >> i) & 1U;
        for (int j = 0; j < NUM_REGS; j++) regs[j] ^= fbit;
        for (int j = 0; j < NUM_REGS; j++) regs[j] = clock_register(regs[j], masks[j]);
    }
    for (int i = 0; i < 384; i++) {
        clock_ctrl_regs_a51(&regs[NUM_REGS_MAIN], &masks[NUM_REGS_MAIN], maj_bit_pos_ctrl);
        (void)clock_main_regs_modified(&regs[0], &masks[0], &regs[NUM_REGS_MAIN], &masks[NUM_REGS_MAIN],
                                      out_bit_pos + NUM_REGS_MAIN, maj_bit_pos_ctrl);
        if (state39.global_out_counter == state39.global_choice) {
            apply_selector2_and_update_choice(regs, masks, out_bit_pos, maj_bit_pos_ctrl);
        }
        state39.global_out_counter++;
    }
}
static void derive_key80_from_key32_and_frame(const uint8_t key32[32], uint64_t frame, uint8_t out_key80[80]) {
    unsigned char buffer[40];
    memcpy(buffer, key32, 32);
    for (int i = 0; i < 8; i++) buffer[32 + i] = (unsigned char)((frame >> (8*i)) & 0xFF);
    unsigned char h4[n1];
    n1global=80;
    MD2II_init();
    MD2II_hashing(buffer, 40);
    MD2II_end(h4);
    for (int i=0;i<80;i++) out_key80[i]=h4[i];

}

static void init(uint8_t key32[32], uint64_t frame) {
    uint8_t key80[80]; memset(key80, 0, sizeof(key80));
    derive_key80_from_key32_and_frame(key32, frame, key80);
    select_10_masks_md2(key80, state39.masks, state39.out_bit_pos, state39.maj_bit_pos_ctrl);
    init_regs(state39.regs, state39.masks, key80, frame, state39.out_bit_pos, state39.maj_bit_pos_ctrl);
}

static void outputbytes7(uint8_t output[7]) {
    memset(output, 0, 7);
    for (int i = 0; i < 56; i++) {
        uint64_t *regs_main = state39.regs;
        uint64_t *regs_ctrl = &state39.regs[NUM_REGS_MAIN];
        uint64_t *masks_main = state39.masks;
        uint64_t *masks_ctrl = &state39.masks[NUM_REGS_MAIN];
        uint8_t *ctrl_out_pos = &state39.out_bit_pos[NUM_REGS_MAIN];
        (void)clock_ctrl_regs_a51(regs_ctrl, masks_ctrl, state39.maj_bit_pos_ctrl);
        (void)clock_main_regs_modified(regs_main, masks_main, regs_ctrl, masks_ctrl, ctrl_out_pos, state39.maj_bit_pos_ctrl);
        uint8_t ks_bit = 0;
        for (int j = 0; j < NUM_REGS_MAIN; j++) {
            ks_bit ^= get_bit(state39.regs[j], state39.out_bit_pos[j]);
        }
        int byte_idx = i / 8;
        int bit_in_byte = 7 - (i % 8);
        output[byte_idx] |= (ks_bit << bit_in_byte);
        if (state39.global_out_counter == state39.global_choice) {
            apply_selector2_and_update_choice(state39.regs, state39.masks, state39.out_bit_pos, state39.maj_bit_pos_ctrl);
        }
        state39.global_out_counter++;
    }
}

void kirisun_top_keystream_creation(dsd_state *state)
{
  uint8_t slot = 0;
  if (state->currentslot == 0)
    slot = 0;
  else slot = 1;

  uint32_t lfsr = 0;
  if (state->currentslot == 0)
    lfsr = (uint32_t)state->payload_mi;
  else lfsr = (uint32_t)state->payload_miR;

  unsigned char userkeyhexa[32]; memset(userkeyhexa, 0, sizeof(userkeyhexa));
  unsigned char realkey[32]; memset(realkey, 0, sizeof(realkey));
  uint8_t ks_bytes[126]; memset(ks_bytes, 0, sizeof(ks_bytes));

  uint64_t K1 = state->A1[slot];
  uint64_t K2 = state->A2[slot];
  uint64_t K3 = state->A3[slot];
  uint64_t K4 = state->A4[slot];

  if (K1 != 0 && K2 != 0 && K3 != 0 && K4 != 0)
    state->aes_key_loaded[slot] = 1;
  else state->aes_key_loaded[slot] = 0;

  if (state->aes_key_loaded[slot] == 1)
  {
    u64_to_bytes_be(K1, &userkeyhexa[0]);
    u64_to_bytes_be(K2, &userkeyhexa[8]);
    u64_to_bytes_be(K3, &userkeyhexa[16]);
    u64_to_bytes_be(K4, &userkeyhexa[24]);

    for(int i=0;i<32;i++) realkey[i]=userkeyhexa[i];

    // printf("=== userkey input (32 bytes) ===\n");
    // print_hex(userkeyhexa,32);

    /* --- MD2II first pass --- */
    n1global=32;
    unsigned char h4[n1]; memset(h4, 0, sizeof(h4));
    MD2II_init();
    MD2II_hashing(realkey,32);
    MD2II_end(h4);
    for(int i=0;i<32;i++) realkey[i]=h4[i];

    // printf("=== realkey final (32 bytes) ===\n");
    // print_hex(realkey,32);

    uint32_t lfsr1 = kirisun_lfsr(lfsr);
    uint64_t frame = ((uint64_t)lfsr1 << 32) | lfsr1;
    init(realkey, frame);

    for (int i = 0; i < 18; i++)
      outputbytes7(ks_bytes+(i*7));

    if (slot == 0)
      memcpy(state->ks_octetL, ks_bytes, sizeof(ks_bytes));
    else memcpy(state->ks_octetR, ks_bytes, sizeof(ks_bytes));

    //debug ks_bytes
    // fprintf (stderr, "KS: ");
    // for (int i = 0; i < 126; i++)
    // {
    //   if ((i != 0) && ((i%16)==0))
    //     fprintf (stderr, "\n    ");
    //   fprintf (stderr, "%02X ", ks_bytes[i]);
    // }
    // fprintf (stderr, "\n");

  }
}

#define BLOCK_BYTES 512
#define MEMORY_BYTES 4608

static uint8_t memory_area[MEMORY_BYTES];
static uint8_t *substitution_area = memory_area;
static uint8_t *state_area = memory_area + 2048;
static uint8_t *feedback_area = memory_area + 3584;
static uint8_t *temporary_area = memory_area + 4096;
static size_t processed_bytes;
static uint8_t feedback_value;

static const uint8_t substitution_maps[16][16] = {
    { 0x4, 0xA, 0x9, 0x2, 0xD, 0x8, 0x0, 0xE, 0x6, 0xB, 0x1, 0xC, 0x7, 0xF, 0x5, 0x3 },
    { 0xE, 0xB, 0x4, 0xC, 0x6, 0xD, 0xF, 0xA, 0x2, 0x3, 0x8, 0x1, 0x0, 0x7, 0x5, 0x9 },
    { 0x5, 0x8, 0x1, 0xD, 0xA, 0x3, 0x4, 0x2, 0xE, 0xF, 0xC, 0x7, 0x6, 0x0, 0x9, 0xB },
    { 0x7, 0xD, 0xA, 0x1, 0x0, 0x8, 0x9, 0xF, 0xE, 0x4, 0x6, 0xC, 0xB, 0x2, 0x5, 0x3 },
    { 0x6, 0xC, 0x7, 0x1, 0x5, 0xF, 0xD, 0x8, 0x4, 0xA, 0x9, 0xE, 0x0, 0x3, 0xB, 0x2 },
    { 0x4, 0xB, 0xA, 0x0, 0x7, 0x2, 0x1, 0xD, 0x3, 0x6, 0x8, 0x5, 0x9, 0xC, 0xF, 0xE },
    { 0xD, 0xB, 0x4, 0x1, 0x3, 0xF, 0x5, 0x9, 0x0, 0xA, 0xE, 0x7, 0x6, 0x8, 0x2, 0xC },
    { 0x1, 0xF, 0xD, 0x0, 0x5, 0x7, 0xA, 0x4, 0x9, 0x2, 0x3, 0xE, 0x6, 0xB, 0x8, 0xC },
    { 0xC, 0x4, 0x6, 0x2, 0xA, 0x5, 0xB, 0x9, 0xE, 0x8, 0xD, 0x7, 0x0, 0x3, 0xF, 0x1 },
    { 0x6, 0x8, 0x2, 0x3, 0x9, 0xA, 0x5, 0xC, 0x1, 0xE, 0x4, 0x7, 0xB, 0xD, 0x0, 0xF },
    { 0xB, 0x3, 0x5, 0x8, 0x2, 0xF, 0xA, 0xD, 0xE, 0x1, 0x7, 0x4, 0xC, 0x9, 0x6, 0x0 },
    { 0xC, 0x8, 0x2, 0x1, 0xD, 0x4, 0xF, 0x6, 0x7, 0x0, 0xA, 0x5, 0x3, 0xE, 0x9, 0xB },
    { 0x7, 0xF, 0x5, 0xA, 0x8, 0x1, 0x6, 0xD, 0x0, 0x9, 0x3, 0xE, 0xB, 0x4, 0x2, 0xC },
    { 0x5, 0xD, 0xF, 0x6, 0x9, 0x2, 0xC, 0xA, 0xB, 0x7, 0x8, 0x1, 0x4, 0x3, 0xE, 0x0 },
    { 0x8, 0xE, 0x2, 0x5, 0x6, 0x9, 0x1, 0xC, 0xF, 0x4, 0xB, 0x0, 0xD, 0xA, 0x3, 0x7 },
    { 0x1, 0x7, 0xE, 0xD, 0x0, 0x5, 0x8, 0x3, 0x4, 0xF, 0xA, 0x6, 0x9, 0xC, 0xB, 0x2 }
};

static const uint8_t nonlinear_map[256] = {
       13, 199,  11,  67, 237, 193, 164,  77, 115, 184, 141, 222,  73,
        38, 147,  36, 150,  87,  21, 104,  12,  61, 156, 101, 111, 145,
       119,  22, 207,  35, 198,  37, 171, 167,  80,  30, 219,  28, 213,
       121,  86,  29, 214, 242,   6,   4,  89, 162, 110, 175,  19, 157,
         3,  88, 234,  94, 144, 118, 159, 239, 100,  17, 182, 173, 238,
        68,  16,  79, 132,  54, 163,  52,   9,  58,  57,  55, 229, 192,
       170, 226,  56, 231, 187, 158,  70, 224, 233, 245,  26,  47,  32,
        44, 247,   8, 251,  20, 197, 185, 109, 153, 204, 218,  93, 178,
       212, 137,  84, 174,  24, 120, 130, 149,  72, 180, 181, 208, 255,
       189, 152,  18, 143, 176,  60, 249,  27, 227, 128, 139, 243, 253,
        59, 123, 172, 108, 211,  96, 138,  10, 215,  42, 225,  40,  81,
        65,  90,  25,  98, 126, 154,  64, 124, 116, 122,   5,   1, 168,
        83, 190, 131, 191, 244, 240, 235, 177, 155, 228, 125,  66,  43,
       201, 248, 220, 129, 188, 230,  62,  75,  71,  78,  34,  31, 216,
       254, 136,  91, 114, 106,  46, 217, 196,  92, 151, 209, 133,  51,
       236,  33, 252, 127, 179,  69,   7, 183, 105, 146,  97,  39,  15,
       205, 112, 200, 166, 223,  45,  48, 246, 186,  41, 148, 140, 107,
        76,  85,  95, 194, 142,  50,  49, 134,  23, 135, 169, 221, 210,
       203,  63, 165,  82, 161, 202,  53,  14, 206, 232, 103, 102, 195,
       117, 250,  99,   0,  74, 160, 241,   2, 113};


void prepare_tables(void)
{
    for (unsigned int index = 0; index < 256; ++index) {
        substitution_area[7 * 256 + index] = (uint8_t)((substitution_maps[15][index >> 4] << 4) | substitution_maps[14][index & 15]);
        substitution_area[6 * 256 + index] = (uint8_t)((substitution_maps[13][index >> 4] << 4) | substitution_maps[12][index & 15]);
        substitution_area[5 * 256 + index] = (uint8_t)((substitution_maps[11][index >> 4] << 4) | substitution_maps[10][index & 15]);
        substitution_area[4 * 256 + index] = (uint8_t)((substitution_maps[9][index >> 4] << 4) | substitution_maps[8][index & 15]);
        substitution_area[3 * 256 + index] = (uint8_t)((substitution_maps[7][index >> 4] << 4) | substitution_maps[6][index & 15]);
        substitution_area[2 * 256 + index] = (uint8_t)((substitution_maps[5][index >> 4] << 4) | substitution_maps[4][index & 15]);
        substitution_area[1 * 256 + index] = (uint8_t)((substitution_maps[3][index >> 4] << 4) | substitution_maps[2][index & 15]);
        substitution_area[index] = (uint8_t)((substitution_maps[1][index >> 4] << 4) | substitution_maps[0][index & 15]);
    }
}

void reset_state(void)
{
    processed_bytes = 0;
    feedback_value = 0;
    memset(feedback_area, 0, 512);
    memset(state_area, 0, 1536);
}

void md2IIult(const uint8_t input[], size_t length)
{
    size_t input_position = 0;
    while (length != 0) {
        for (; length && processed_bytes < BLOCK_BYTES; --length, ++processed_bytes) {
            uint8_t current_byte = input[input_position++];
            state_area[processed_bytes + BLOCK_BYTES] = current_byte;
            state_area[processed_bytes + 2 * BLOCK_BYTES] = (uint8_t)(current_byte ^ state_area[processed_bytes]);
            feedback_area[processed_bytes] ^= nonlinear_map[current_byte ^ feedback_value];
            feedback_value = feedback_area[processed_bytes];
        }
        if (processed_bytes == BLOCK_BYTES) {
            unsigned int accumulator = 0;
            processed_bytes = 0;
            for (unsigned int round = 0; round < BLOCK_BYTES + 2; ++round) {
                for (unsigned int index = 0; index < 3 * BLOCK_BYTES; ++index) {
                    state_area[index] ^= nonlinear_map[accumulator];
                    accumulator = state_area[index];
                }
                accumulator = (accumulator + round) & 255U;
            }
        }
    }
}

void finalize_state(uint8_t output[BLOCK_BYTES])
{
    size_t remaining_bytes = BLOCK_BYTES - processed_bytes;
    memset(temporary_area, (int)remaining_bytes, remaining_bytes);
    md2IIult(temporary_area, remaining_bytes);
    md2IIult(feedback_area, BLOCK_BYTES);
    memcpy(output, state_area, BLOCK_BYTES);
}

void derive_schedule(const uint8_t digest[BLOCK_BYTES], uint8_t schedule[BLOCK_BYTES])
{
    size_t digest_position = 0;
    memset(schedule, 0, BLOCK_BYTES);
    for (unsigned int block = 0; block < 64; ++block) {
        uint64_t value = 0;
        for (unsigned int index = 0; index < 8; ++index) value = (value << 8) | digest[digest_position++];
        memcpy(schedule + block * 8, &value, sizeof(value));
    }
}

static uint64_t transform_word(uint64_t value)
{
    uint32_t upper_word = (uint32_t)(value >> 32);
    uint32_t lower_word = (uint32_t)value;
    upper_word = ((uint32_t)substitution_area[3 * 256 + ((upper_word >> 24) & 255u)] << 24) |
                 ((uint32_t)substitution_area[2 * 256 + ((upper_word >> 16) & 255u)] << 16) |
                 ((uint32_t)substitution_area[1 * 256 + ((upper_word >> 8) & 255u)] << 8) |
                 (uint32_t)substitution_area[upper_word & 255u];
    lower_word = ((uint32_t)substitution_area[7 * 256 + ((lower_word >> 24) & 255u)] << 24) |
                 ((uint32_t)substitution_area[6 * 256 + ((lower_word >> 16) & 255u)] << 16) |
                 ((uint32_t)substitution_area[5 * 256 + ((lower_word >> 8) & 255u)] << 8) |
                 (uint32_t)substitution_area[4 * 256 + (lower_word & 255u)];
    value = ((uint64_t)upper_word << 32) | lower_word;
    return (value << 11) | (value >> (64 - 11));
}

void encode_block(const uint64_t input[2], uint64_t output[2], const uint8_t schedule[BLOCK_BYTES])
{
    uint64_t left_word = input[0], right_word = input[1];
    for (unsigned int round = 0; round < 32; ++round) {
        uint64_t round_key;
        memcpy(&round_key, schedule + round * 16, sizeof(round_key));
        right_word ^= transform_word(left_word + round_key);
        memcpy(&round_key, schedule + round * 16 + 8, sizeof(round_key));
        left_word ^= transform_word(right_word + round_key);
    }
    output[0] = right_word;
    output[1] = left_word;
}

void decode_block(const uint64_t input[2], uint64_t output[2], const uint8_t schedule[BLOCK_BYTES])
{
    uint64_t left_word = input[0], right_word = input[1];
    for (int round = 31; round >= 0; --round) {
        uint64_t round_key;
        memcpy(&round_key, schedule + round * 16 + 8, sizeof(round_key));
        right_word ^= transform_word(left_word + round_key);
        memcpy(&round_key, schedule + round * 16, sizeof(round_key));
        left_word ^= transform_word(right_word + round_key);
    }
    output[0] = right_word;
    output[1] = left_word;
}

void kirisun_ultra_keystream_creation(dsd_state *state)
{
  uint8_t slot = 0;
  if (state->currentslot == 0)
    slot = 0;
  else slot = 1;

  uint32_t lfsr = 0;
  if (state->currentslot == 0)
    lfsr = (uint32_t)state->payload_mi;
  else lfsr = (uint32_t)state->payload_miR;

  unsigned char userkeyhexa[32]; memset(userkeyhexa, 0, sizeof(userkeyhexa));
  unsigned char realkey[32]; memset(realkey, 0, sizeof(realkey));
  unsigned char lfsrhexa[4]; memset(lfsrhexa, 0, sizeof(lfsrhexa));
  uint8_t ks_bytes[128]; memset(ks_bytes, 0, sizeof(ks_bytes));

  lfsrhexa[0] = (lfsr >> 24) & 0xFF;
  lfsrhexa[1] = (lfsr >> 16) & 0xFF;
  lfsrhexa[2] = (lfsr >> 8)  & 0xFF;
  lfsrhexa[3] = (lfsr >> 0)  & 0xFF;

  uint64_t K1 = state->A1[slot];
  uint64_t K2 = state->A2[slot];
  uint64_t K3 = state->A3[slot];
  uint64_t K4 = state->A4[slot];

  if (K1 != 0 && K2 != 0 && K3 != 0 && K4 != 0)
    state->aes_key_loaded[slot] = 1;
  else state->aes_key_loaded[slot] = 0;

  if (state->aes_key_loaded[slot] == 1)
  {

    u64_to_bytes_be(K1, &userkeyhexa[0]);
    u64_to_bytes_be(K2, &userkeyhexa[8]);
    u64_to_bytes_be(K3, &userkeyhexa[16]);
    u64_to_bytes_be(K4, &userkeyhexa[24]);

    for(int i=0;i<32;i++) realkey[i]=userkeyhexa[i];

    // printf("=== userkey input (32 bytes) ===\n");
    // print_hex(userkeyhexa,32);

    /* --- MD2II first pass --- */
    n1global=32;
    unsigned char h4[n1]; memset(h4, 0, sizeof(h4));
    MD2II_init();
    MD2II_hashing(realkey,32);
    MD2II_end(h4);
    for(int i=0;i<32;i++) realkey[i]=h4[i];

    // printf("=== realkey final (32 bytes) ===\n");
    // print_hex(realkey,32);

    unsigned char digest[512]; memset(digest, 0, sizeof(digest));
    unsigned char schedule[512]; memset(schedule, 0, sizeof(schedule));

    prepare_tables();
    reset_state();
    md2IIult(realkey, 32);
    finalize_state(digest);
    derive_schedule(digest, schedule);

    n1global=32;
    MD2II_init();
    MD2II_hashing(lfsrhexa,4);
    MD2II_hashing(realkey,32);
    MD2II_end(h4);

    // printf("h4[0..7]: ");
    // print_hex(h4,8);

    //the state machine is comprised of two 64-bit words that
    //when output become the bytes of the keystream
    uint64_t internal_state[2]; memset(internal_state, 0, sizeof(internal_state));

    for (int i = 0; i < 8; i++)
    {
      internal_state[0] <<= 8;
      internal_state[0] |= (uint64_t)h4[i];

      internal_state[1] <<= 8;
      internal_state[1] |= (uint64_t)h4[i+8];
    }

    /* --- keystream output --- */
    uint8_t output[128]; memset(output, 0, sizeof(output));

    for (int i = 0; i < 8; i++)
    {
      encode_block(internal_state, internal_state, schedule);

      output[(i*16)+0] = (internal_state[0] >> 56) & 0xFF;
      output[(i*16)+1] = (internal_state[0] >> 48) & 0xFF;
      output[(i*16)+2] = (internal_state[0] >> 40) & 0xFF;
      output[(i*16)+3] = (internal_state[0] >> 32) & 0xFF;
      output[(i*16)+4] = (internal_state[0] >> 24) & 0xFF;
      output[(i*16)+5] = (internal_state[0] >> 16) & 0xFF;
      output[(i*16)+6] = (internal_state[0] >>  8) & 0xFF;
      output[(i*16)+7] = (internal_state[0] >>  0) & 0xFF;

      output[(i*16)+8]  = (internal_state[1] >> 56) & 0xFF;
      output[(i*16)+9]  = (internal_state[1] >> 48) & 0xFF;
      output[(i*16)+10] = (internal_state[1] >> 40) & 0xFF;
      output[(i*16)+11] = (internal_state[1] >> 32) & 0xFF;
      output[(i*16)+12] = (internal_state[1] >> 24) & 0xFF;
      output[(i*16)+13] = (internal_state[1] >> 16) & 0xFF;
      output[(i*16)+14] = (internal_state[1] >>  8) & 0xFF;
      output[(i*16)+15] = (internal_state[1] >>  0) & 0xFF;

    }

    // printf("keystream output (128 bytes):\n");
    // print_hex(output,128);

    //store ks_bytes to storage
    for (int i = 0; i < 128; i++)
      ks_bytes[i] = output[i];

    if (slot == 0)
      memcpy(state->ks_octetL, ks_bytes, sizeof(ks_bytes));
    else memcpy(state->ks_octetR, ks_bytes, sizeof(ks_bytes));

    //debug ks_bytes
    // fprintf (stderr, "KS: ");
    // for (int i = 0; i < 128; i++)
    // {
    //   if ((i != 0) && ((i%16)==0))
    //     fprintf (stderr, "\n    ");
    //   fprintf (stderr, "%02X ", ks_bytes[i]);
    // }
    // fprintf (stderr, "\n");

  }
}

//General Keystream Creator for any known Kirisun cipher
void kirisun_keystream_creation(dsd_state * state)
{

  uint8_t algid = 0;
  if (state->currentslot == 0)
    algid = state->payload_algid;
  else algid = state->payload_algidR;

  if (algid == 0x31)
    kirisun_reinforced_keystream_creation(state);

  else if (algid == 0x32)
    kirisun_low_keystream_creation(state);

  else if (algid == 0x33)
    kirisun_middle_keystream_creation(state);

  else if (algid == 0x34)
    kirisun_high_keystream_creation(state);

  else if (algid == 0x35)
    kirisun_enhanced_keystream_creation(state);

  else if (algid == 0x36)
    kirisun_advanced_keystream_creation(state);

  else if (algid == 0x37)
    kirisun_universal_keystream_creation(state);

  else if (algid == 0x38)
    kirisun_strengthened_keystream_creation(state);

  else if (algid == 0x39)
    kirisun_top_keystream_creation(state);
  
  else if (algid == 0x3A)
    kirisun_ultra_keystream_creation(state);

}
