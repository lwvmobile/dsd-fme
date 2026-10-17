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

void kirisun_top_keystream_creation(dsd_state *state)
{
  UNUSED(state);
}

void kirisun_ultimate_keystream_creation(dsd_state *state)
{
  UNUSED(state);
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
    kirisun_ultimate_keystream_creation(state);

}
