#include "pc5.h"
#include "dsd.h"

#include <ctype.h>

/* Global PC5 context instance */
PC5Context ctxpc5;

/* ---------------------------------
   Internal utility functions
----------------------------------- */

/* Rotate right */
static uint32_t ror(uint32_t x, int shift, int bits) {
    uint32_t m0 = (1u << (bits - shift)) - 1u;
    uint32_t m1 = (1u << shift) - 1u;
    return ((x >> shift) & m0) | ((x & m1) << (bits - shift));
}

/* Rotate left */
static uint32_t rol(uint32_t x, int shift, int bits) {
    uint32_t m0 = (1u << (bits - shift)) - 1u;
    uint32_t m1 = (1u << shift) - 1u;
    return ((x & m0) << shift) | ((x >> (bits - shift)) & m1);
}

/* SplitMix64 random number generator */
static uint64_t next_rng(PC5Context *ctxpc5) {
    uint64_t z = (ctxpc5->x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

/* ARC4 initialization */
static void arc4_init(PC5Context *ctxpc5, unsigned char key[]) {
    int tmp;
    for (ctxpc5->i_arc4 = 0; ctxpc5->i_arc4 < 256; ctxpc5->i_arc4++)
        ctxpc5->array_arc4[ctxpc5->i_arc4] = (unsigned char)ctxpc5->i_arc4;

    ctxpc5->j_arc4 = 0;
    for (ctxpc5->i_arc4 = 0; ctxpc5->i_arc4 < 256; ctxpc5->i_arc4++) {
        ctxpc5->j_arc4 = (ctxpc5->j_arc4 + ctxpc5->array_arc4[ctxpc5->i_arc4] + key[ctxpc5->i_arc4 % 256]) % 256;
        tmp = ctxpc5->array_arc4[ctxpc5->i_arc4];
        ctxpc5->array_arc4[ctxpc5->i_arc4] = ctxpc5->array_arc4[ctxpc5->j_arc4];
        ctxpc5->array_arc4[ctxpc5->j_arc4] = tmp;
    }
    ctxpc5->i_arc4 = 0;
    ctxpc5->j_arc4 = 0;
}

/* ARC4 output combined with SplitMix64 stream */
static unsigned char arc4_output(PC5Context *ctxpc5) {
    uint8_t rndbyte, decal;
    int tmp, t;

    ctxpc5->i_arc4 = (ctxpc5->i_arc4 + 1) % 256;
    ctxpc5->j_arc4 = (ctxpc5->j_arc4 + ctxpc5->array_arc4[ctxpc5->i_arc4]) % 256;
    tmp = ctxpc5->array_arc4[ctxpc5->i_arc4];
    ctxpc5->array_arc4[ctxpc5->i_arc4] = ctxpc5->array_arc4[ctxpc5->j_arc4];
    ctxpc5->array_arc4[ctxpc5->j_arc4] = tmp;
    t = (ctxpc5->array_arc4[ctxpc5->i_arc4] + ctxpc5->array_arc4[ctxpc5->j_arc4]) % 256;

    if (ctxpc5->xyz == 0) ctxpc5->bb = next_rng(ctxpc5);
    decal = (uint8_t)(56 - (8 * ctxpc5->xyz));
    rndbyte = (uint8_t)((ctxpc5->bb >> decal) & 0xffu);
    ctxpc5->xyz++;
    if (ctxpc5->xyz == 8) ctxpc5->xyz = 0;

    if (ctxpc5->count == 0) {
        rndbyte = (uint8_t)(rndbyte ^ ctxpc5->array_arc4[t]);
        ctxpc5->count = 1;
    } else {
        rndbyte = (uint8_t)(rndbyte + ctxpc5->array_arc4[t]);
        ctxpc5->count = 0;
    }
    return rndbyte;
}

/* Initialize MD2-II state */
static void md2_init(PC5Context *ctxpc5) {
    ctxpc5->x1 = 0;
    ctxpc5->x2 = 0;
    for (ctxpc5->i = 0; ctxpc5->i < n1; ctxpc5->i++) ctxpc5->h2[ctxpc5->i] = 0;
    for (ctxpc5->i = 0; ctxpc5->i < n1; ctxpc5->i++) ctxpc5->h1[ctxpc5->i] = 0;
}

/* MD2-II hashing */
static void md2_hashing(PC5Context *ctxpc5, unsigned char t1[], size_t b6) {
    static const unsigned char s4[256] = {
        13,199,11,67,237,193,164,77,115,184,141,222,73,38,147,36,150,87,21,104,12,61,156,101,111,145,
        119,22,207,35,198,37,171,167,80,30,219,28,213,121,86,29,214,242,6,4,89,162,110,175,19,157,3,
        88,234,94,144,118,159,239,100,17,182,173,238,68,16,79,132,54,163,52,9,58,57,55,229,192,170,226,
        56,231,187,158,70,224,233,245,26,47,32,44,247,8,251,20,197,185,109,153,204,218,93,178,212,137,84,
        174,24,120,130,149,72,180,181,208,255,189,152,18,143,176,60,249,27,227,128,139,243,253,59,123,172,
        108,211,96,138,10,215,42,225,40,81,65,90,25,98,126,154,64,124,116,122,5,1,168,83,190,131,191,244,
        240,235,177,155,228,125,66,43,201,248,220,129,188,230,62,75,71,78,34,31,216,254,136,91,114,106,46,
        217,196,92,151,209,133,51,236,33,252,127,179,69,7,183,105,146,97,39,15,205,112,200,166,223,45,48,
        246,186,41,148,140,107,76,85,95,194,142,50,49,134,23,135,169,221,210,203,63,165,82,161,202,53,14,
        206,232,103,102,195,117,250,99,0,74,160,241,2,113};

    int b1, b2, b3, b4, b5;
    b4 = 0;
    while (b6) {
        for (; b6 && ctxpc5->x2 < n1; b6--, ctxpc5->x2++) {
            b5 = t1[b4++];
            ctxpc5->h1[ctxpc5->x2 + n1] = (unsigned char)b5;
            ctxpc5->h1[ctxpc5->x2 + (n1 * 2)] = (unsigned char)(b5 ^ ctxpc5->h1[ctxpc5->x2]);
            ctxpc5->x1 = ctxpc5->h2[ctxpc5->x2] ^= s4[b5 ^ ctxpc5->x1];
        }
        if (ctxpc5->x2 == n1) {
            b2 = 0;
            ctxpc5->x2 = 0;
            for (b3 = 0; b3 < (n1 + 2); b3++) {
                for (b1 = 0; b1 < (n1 * 3); b1++)
                    b2 = ctxpc5->h1[b1] ^= s4[b2];
                b2 = (b2 + b3) % 256;
            }
        }
    }
}

/* Finalize MD2-II */
static void md2_end(PC5Context *ctxpc5, unsigned char h4[n1]) {
    unsigned char h3[n1];
    int i, n4 = n1 - ctxpc5->x2;
    for (i = 0; i < n4; i++) h3[i] = (unsigned char)n4;
    md2_hashing(ctxpc5, h3, (size_t)n4);
    md2_hashing(ctxpc5, ctxpc5->h2, sizeof(ctxpc5->h2));
    for (i = 0; i < n1; i++) h4[i] = ctxpc5->h1[i];
}

/* Generate a random index */
static int mixy(PC5Context *ctxpc5, int nn2) {
    return arc4_output(ctxpc5) % nn2;
}

/* Fisher-Yates shuffle */
static void mixer(PC5Context *ctxpc5, uint8_t *mixu, int nn) {
    int ii, jj, tmmp;
    for (ii = nn - 1; ii > 0; ii--) {
        jj = mixy(ctxpc5, ii + 1);
        tmmp = mixu[jj];
        mixu[jj] = mixu[ii];
        mixu[ii] = (uint8_t)tmmp;
    }
}

/* Key schedule and S-box generation */
void create_keys_pc5(PC5Context *ctxpc5, unsigned char key1[], size_t size1) {
    int i, w, k;
    unsigned char h4[n1];

    md2_init(ctxpc5);
    md2_hashing(ctxpc5, key1, size1);
    md2_end(ctxpc5, h4);

    for (i = 0; i < 16; i++) ctxpc5->keys[i] = h4[i];
    arc4_init(ctxpc5, h4);

    ctxpc5->x = 0;
    for (i = 0; i < 8; i++) ctxpc5->x = (ctxpc5->x << 8) + (uint64_t)(h4[256 + i] & 0xffu);

    ctxpc5->xyz = 0;
    ctxpc5->count = 0;

    for (i = 0; i < 23000; i++) (void)arc4_output(ctxpc5);

    uint8_t numbers[256];
    
    for (w = 0; w < 253; w++) {
        k = arc4_output(ctxpc5) + 256;
        for (i = 0; i < k; i++) (void)arc4_output(ctxpc5);
        for (i = 0; i < 16; i++) numbers[i] = (uint8_t)i;
        mixer(ctxpc5, numbers, 16);
        for (i = 0; i < 16; i++) ctxpc5->perm[i][w] = numbers[i];
    }
        
    k = arc4_output(ctxpc5) + 256;
    for (i = 0; i < k; i++) (void)arc4_output(ctxpc5);
    for (i = 0; i < 16; i++) numbers[i] = (uint8_t)i;
    mixer(ctxpc5, numbers, 16);
    for (i = 0; i < 16; i++) ctxpc5->new1[i] = numbers[i];

    k = arc4_output(ctxpc5) + 256;
    for (i = 0; i < k; i++) (void)arc4_output(ctxpc5);
    for (i = 0; i < nbround; i++) ctxpc5->decal[i] = (uint8_t)((arc4_output(ctxpc5) % 11) + 1);

    k = arc4_output(ctxpc5) + 256;
    for (i = 0; i < k; i++) (void)arc4_output(ctxpc5);
    for (w = 0; w < 3; w++)
        for (i = 0; i < nbround; i++)
            ctxpc5->rngxor[i][w] = arc4_output(ctxpc5) % 16;

    k = arc4_output(ctxpc5) + 256;
    for (i = 0; i < k; i++) (void)arc4_output(ctxpc5);
    for (i = 0; i < 16; i++) numbers[i] = (uint8_t)i;
    mixer(ctxpc5, numbers, 16);
    for (i = 0; i < 16; i++) {
        ctxpc5->tab[i] = numbers[i];
        ctxpc5->inv[ctxpc5->tab[i]] = (unsigned char)i;
    }

    k = arc4_output(ctxpc5) + 256;
    for (i = 0; i < k; i++) (void)arc4_output(ctxpc5);
    for (w = 0; w < 3; w++) {
        k = arc4_output(ctxpc5) + 256;
        for (i = 0; i < k; i++) (void)arc4_output(ctxpc5);
        for (i = 0; i < 3; i++) numbers[i] = (uint8_t)i;
        mixer(ctxpc5, numbers, 3);
        for (i = 0; i < 3; i++) ctxpc5->permut[w][i] = numbers[i];
    }
    
    

    k = arc4_output(ctxpc5) + 256;
    for (i = 0; i < k; i++) (void)arc4_output(ctxpc5);
    for (w = 0; w < 3; w++)
        for (i = 0; i < nbround; i++)
            ctxpc5->rngxor2[i][w] = arc4_output(ctxpc5) % 16;
            
   k = arc4_output(ctxpc5) + 256;

   for (w = 0; w < 25; w++) ctxpc5->numbers[w]=arc4_output(ctxpc5) % 2;
   
}

static void compute_pc5(PC5Context *ctxpc5, uint8_t *tab1, uint8_t round) {
    ctxpc5->tot[0] = (uint8_t)((ctxpc5->perm[tab1[ctxpc5->permut[0][0]]][round] +
                             ctxpc5->perm[tab1[ctxpc5->permut[0][1]]][round]) ^
                            ctxpc5->perm[tab1[ctxpc5->permut[0][2]]][round]);
    ctxpc5->tot[0] = (uint8_t)((ctxpc5->tot[0] + ctxpc5->new1[ctxpc5->tot[0]]) % 16);
    ctxpc5->tot[1] = (uint8_t)((ctxpc5->perm[tab1[ctxpc5->permut[1][0]]][round] +
                             ctxpc5->perm[tab1[ctxpc5->permut[1][1]]][round]) ^
                            ctxpc5->perm[tab1[ctxpc5->permut[1][2]]][round]);
    ctxpc5->tot[1] = (uint8_t)((ctxpc5->tot[1] + ctxpc5->new1[ctxpc5->tot[1]]) % 16);
    ctxpc5->tot[2] = (uint8_t)((ctxpc5->perm[tab1[ctxpc5->permut[2][0]]][round] +
                             ctxpc5->perm[tab1[ctxpc5->permut[2][1]]][round]) ^
                            ctxpc5->perm[tab1[ctxpc5->permut[2][2]]][round]);
    ctxpc5->tot[2] = (uint8_t)((ctxpc5->tot[2] + ctxpc5->new1[ctxpc5->tot[2]]) % 16);
}

/* Convert bits to bytes */
void binhexpc5(PC5Context *ctxpc5, short *z, int length) {
    short *b = (short *)z;
    uint8_t i, j;
    for (i = 0; i < length; i = j) {
        uint8_t a = 0;
        for (j = i; j < i + 8; ++j) {
            a |= (uint8_t)(b[((short)(7 - (j % 8)) + j) - (j % 8)] << (j - i));
        }
        ctxpc5->convert[ctxpc5->ptconvert] = a;
        ctxpc5->ptconvert++;
    }
}

/* Convert byte to bits */
void hexbinpc5(PC5Context *ctxpc5, short *q, uint8_t w, uint8_t hex) { // warning: unused parameter ‘ctxpc5’ [-Wunused-parameter]
    UNUSED(ctxpc5);
    short *bits = (short *)q;
    for (uint8_t i = 0; i < 8; ++i) {
        bits[(short)(7 + w) - i] = (short)((hex >> i) & 1u);
    }

   // UNUSEDPC4(ctxpc5); //fix above warning
}

/* Encrypt one block */
void pc5encrypt(PC5Context *ctxpc5) {
    int i;
    int g;

    for (i = 0; i < 3; i++) {
        ctxpc5->l[0][i] = ctxpc5->convert[i];
        ctxpc5->r[0][i] = ctxpc5->convert[i + 3];
    }

    for (i = 1; i <= ctxpc5->rounds; i++) {
        ctxpc5->r[(i - 1) % 2][0] = (ctxpc5->r[(i - 1) % 2][0] + ((uint8_t)~ctxpc5->rngxor2[ctxpc5->rounds - i][0])) % 16;
        ctxpc5->r[(i - 1) % 2][1] = (ctxpc5->r[(i - 1) % 2][1] ^ ((uint8_t)~ctxpc5->rngxor2[ctxpc5->rounds - i][1])) % 16;
        ctxpc5->r[(i - 1) % 2][2] = (ctxpc5->r[(i - 1) % 2][2] + ((uint8_t)~ctxpc5->rngxor2[ctxpc5->rounds - i][2])) % 16;

        ctxpc5->result = 0;
        ctxpc5->result += ((uint32_t)ctxpc5->r[(i - 1) % 2][0] << 8);
        ctxpc5->result += ((uint32_t)ctxpc5->r[(i - 1) % 2][1] << 4);
        ctxpc5->result += ctxpc5->r[(i - 1) % 2][2];

        ctxpc5->result = rol(ctxpc5->result, ctxpc5->decal[i - 1], 12);

        ctxpc5->r[(i - 1) % 2][0] = (uint8_t)(ctxpc5->result >> 8);
        ctxpc5->r[(i - 1) % 2][1] = (uint8_t)((ctxpc5->result >> 4) & 0xfu);
        ctxpc5->r[(i - 1) % 2][2] = (uint8_t)(ctxpc5->result & 0xfu);

        ctxpc5->r[(i - 1) % 2][0] = ctxpc5->tab[ctxpc5->r[(i - 1) % 2][0]];
        ctxpc5->r[(i - 1) % 2][0] = (ctxpc5->r[(i - 1) % 2][0] ^ ctxpc5->rngxor[i - 1][0]) % 16;

        ctxpc5->r[(i - 1) % 2][1] = ctxpc5->inv[ctxpc5->r[(i - 1) % 2][1]];
        
        if ((ctxpc5->r[(i - 1) % 2][1] - ctxpc5->rngxor[i - 1][1]) < 0) {
            g = (ctxpc5->r[(i - 1) % 2][1] - ctxpc5->rngxor[i - 1][1]) + 16;
        } else {
            g = (ctxpc5->r[(i - 1) % 2][1] - ctxpc5->rngxor[i - 1][1]) % 16;
        }
        ctxpc5->r[(i - 1) % 2][1] = g;

        ctxpc5->r[(i - 1) % 2][2] = ctxpc5->tab[ctxpc5->r[(i - 1) % 2][2]];
        ctxpc5->r[(i - 1) % 2][2] = (ctxpc5->r[(i - 1) % 2][2] ^ ctxpc5->rngxor[i - 1][2]) % 16;

        compute_pc5(ctxpc5, ctxpc5->r[(i - 1) % 2], (uint8_t)((i - 1) % 253));

        ctxpc5->l[i % 2][0] = ctxpc5->r[(i - 1) % 2][0];
        
        if ((ctxpc5->l[(i - 1) % 2][0] - ctxpc5->tot[0]) < 0) {
            g = (ctxpc5->l[(i - 1) % 2][0] - ctxpc5->tot[0]) + 16;
        } else {
            g = ctxpc5->l[(i - 1) % 2][0] - ctxpc5->tot[0];
        }
        ctxpc5->r[i % 2][0] = g;

        ctxpc5->l[i % 2][1] = ctxpc5->r[(i - 1) % 2][1];
        ctxpc5->r[i % 2][1] = (ctxpc5->l[(i - 1) % 2][1] ^ ctxpc5->tot[1]) % 16;

        ctxpc5->l[i % 2][2] = ctxpc5->r[(i - 1) % 2][2];
        
        if ((ctxpc5->l[(i - 1) % 2][2] - ctxpc5->tot[2]) < 0) {
            g = (ctxpc5->l[(i - 1) % 2][2] - ctxpc5->tot[2]) + 16;
        } else {
            g = ctxpc5->l[(i - 1) % 2][2] - ctxpc5->tot[2];
        }
        ctxpc5->r[i % 2][2] = g;
    }

    for (i = 0; i < 3; i++) {
        ctxpc5->convert[i + 3] = ctxpc5->l[(ctxpc5->rounds - 1) % 2][i];
        ctxpc5->convert[i] = ctxpc5->r[(ctxpc5->rounds - 1) % 2][i];
    }
}

/* Decrypt one block */
void pc5decrypt(PC5Context *ctxpc5) {
    int i;

    for (i = 0; i < 3; i++) {
        ctxpc5->l[0][i] = ctxpc5->convert[i];
        ctxpc5->r[0][i] = ctxpc5->convert[i + 3];
    }

    ctxpc5->y = (uint8_t)((ctxpc5->rounds - 1) % 253);
    if (ctxpc5->y == 0) ctxpc5->y = 253;

    for (i = 1; i <= ctxpc5->rounds; i++) {
        ctxpc5->y--;
        compute_pc5(ctxpc5, ctxpc5->r[(i - 1) % 2], ctxpc5->y);
        if (ctxpc5->y == 0) ctxpc5->y = 253;

        ctxpc5->result = 0;

        ctxpc5->l[(i - 1) % 2][0] ^= ctxpc5->rngxor[ctxpc5->rounds - i][0];
        ctxpc5->l[(i - 1) % 2][0] %= 16;
        ctxpc5->l[(i - 1) % 2][0] = ctxpc5->inv[ctxpc5->l[(i - 1) % 2][0]];

        ctxpc5->l[(i - 1) % 2][1] += ctxpc5->rngxor[ctxpc5->rounds - i][1];
        ctxpc5->l[(i - 1) % 2][1] %= 16;
        ctxpc5->l[(i - 1) % 2][1] = ctxpc5->tab[ctxpc5->l[(i - 1) % 2][1]];

        ctxpc5->l[(i - 1) % 2][2] ^= ctxpc5->rngxor[ctxpc5->rounds - i][2];
        ctxpc5->l[(i - 1) % 2][2] %= 16;
        ctxpc5->l[(i - 1) % 2][2] = ctxpc5->inv[ctxpc5->l[(i - 1) % 2][2]];

        ctxpc5->result += ((uint32_t)ctxpc5->l[(i - 1) % 2][0] << 8);
        ctxpc5->result += ((uint32_t)ctxpc5->l[(i - 1) % 2][1] << 4);
        ctxpc5->result += ctxpc5->l[(i - 1) % 2][2];

        ctxpc5->result = ror(ctxpc5->result, ctxpc5->decal[ctxpc5->rounds - i], 12);

        ctxpc5->l[(i - 1) % 2][0] = (uint8_t)(ctxpc5->result >> 8);
        ctxpc5->l[(i - 1) % 2][1] = (uint8_t)((ctxpc5->result >> 4) & 0xfu);
        ctxpc5->l[(i - 1) % 2][2] = (uint8_t)(ctxpc5->result & 0xfu);

        int temp;
        
        temp = ctxpc5->l[(i - 1) % 2][0] - ((uint8_t)(~ctxpc5->rngxor2[i - 1][0]) % 16);
        if (temp < 0) temp += 16;
        ctxpc5->l[(i - 1) % 2][0] = temp % 16;

        ctxpc5->l[(i - 1) % 2][1] ^= (uint8_t)(~ctxpc5->rngxor2[i - 1][1]) % 16;
        ctxpc5->l[(i - 1) % 2][1] %= 16;

        temp = ctxpc5->l[(i - 1) % 2][2] - ((uint8_t)(~ctxpc5->rngxor2[i - 1][2]) % 16);
        if (temp < 0) temp += 16;
        ctxpc5->l[(i - 1) % 2][2] = temp % 16;

        ctxpc5->l[i % 2][0] = ctxpc5->r[(i - 1) % 2][0];
        ctxpc5->r[i % 2][0] = (ctxpc5->l[(i - 1) % 2][0] + ctxpc5->tot[0]) % 16;

        ctxpc5->l[i % 2][1] = ctxpc5->r[(i - 1) % 2][1];
        ctxpc5->r[i % 2][1] = (ctxpc5->l[(i - 1) % 2][1] ^ ctxpc5->tot[1]) % 16;

        ctxpc5->l[i % 2][2] = ctxpc5->r[(i - 1) % 2][2];
        ctxpc5->r[i % 2][2] = (ctxpc5->l[(i - 1) % 2][2] + ctxpc5->tot[2]) % 16;
    }

    for (i = 0; i < 3; i++) {
        ctxpc5->convert[i + 3] = ctxpc5->l[(ctxpc5->rounds - 1) % 2][i];
        ctxpc5->convert[i] = ctxpc5->r[(ctxpc5->rounds - 1) % 2][i];
    }
}

/* Key creation for BAOFENG AP */
void baofeng_ap_pc5_keystream_creation(dsd_state *state, char *input) {
    
    char buf[1024];
    strncpy(buf, input, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    
    // Count hexadecimal characters (without spaces)
    int hex_char_count = 0;
    for (int i = 0; buf[i] != '\0'; i++) {
        if (isxdigit(buf[i])) {
            hex_char_count++;
        }
    }
    
    if (hex_char_count == 32) { // 128 bits (32 hex characters)
        unsigned char key1[16] = {0};
        unsigned char key2[16] = {0};
        
        char *pEnd;
        uint64_t K1 = strtoull(buf, &pEnd, 16);
        uint64_t K2 = strtoull(pEnd, &pEnd, 16);
        
        u64_to_bytes_be_pc5(K1, &key1[0]);
        u64_to_bytes_be_pc5(K2, &key1[8]);

        // Reverse key for PC5 (only for 128-bit)
        for (int i=0;i<16;i++) key2[i] = key1[15-i];
        
         /* Create key schedule */
        create_keys_pc5(&ctxpc5, key2, sizeof(key2));
        ctxpc5.rounds = nbround;
        
        fprintf(stderr, "DMR BAOFENG AP (PC5) 128-bit Key %016llX%016llX with Forced Application\n", 
                (unsigned long long)K1, (unsigned long long)K2);
        state->baofeng_ap = 1;
        
    } else if (hex_char_count == 64) { // 256 bits (64 hex characters)
        // Remove spaces and store as ASCII string (64 bytes)
        char ascii_key[65] = {0}; // 64 characters + null terminator
        int j = 0;
        for (int i = 0; buf[i] != '\0' && j < 64; i++) {
            if (isxdigit(buf[i])) {
                ascii_key[j++] = buf[i];
            }
        }
        ascii_key[64] = '\0';
        
         /* Create key schedule */
        create_keys_pc5(&ctxpc5, (unsigned char*)ascii_key, 64);
        ctxpc5.rounds = nbround;
        
        fprintf(stderr, "DMR BAOFENG AP (PC5) 256-bit Key: %s with Forced Application\n", ascii_key);
        state->baofeng_ap = 1;
        
    } else {
        fprintf(stderr, "ERROR: Invalid key length. Expected 32 chars (128-bit) or 64 chars (256-bit), got %d hex chars\n", hex_char_count);
        return;
    }
}
