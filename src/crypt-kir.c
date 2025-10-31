#include "dsd.h"
#include "pc4.h"

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

//dsd-fme -fs -Z -i '/SSD_STORAGE/2025_DEV/Tytera BP EP AP Master Folder/kirisun/cap-clear.wav/cap-33.wav' -H '3333333333333333 3333333333333333 3333333333333333 3333333333333333'
void kirisun_uni_keystream_creation(dsd_state *state)
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

//dsd-fme -fs -Z -i '/SSD_STORAGE/2025_DEV/Tytera BP EP AP Master Folder/kirisun/better_samples_tg1/kirisun_advanced_tg1.wav' -H 'DC1A7E9F9BF312DB F45010CEC5F7A53A C407D0BFA803617B E426A7254DA9390D'
void kirisun_adv_keystream_creation(dsd_state *state)
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
