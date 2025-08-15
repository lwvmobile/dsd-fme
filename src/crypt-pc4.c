#include "pc4.h"

/* Global PC4 context instance */
PC4Context ctx;

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
static uint64_t next_rng(PC4Context *ctx) {
    uint64_t z = (ctx->x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

/* ARC4 initialization */
static void arc4_init(PC4Context *ctx, unsigned char key[]) {
    int tmp;
    for (ctx->i_arc4 = 0; ctx->i_arc4 < 256; ctx->i_arc4++)
        ctx->array_arc4[ctx->i_arc4] = (unsigned char)ctx->i_arc4;

    ctx->j_arc4 = 0;
    for (ctx->i_arc4 = 0; ctx->i_arc4 < 256; ctx->i_arc4++) {
        ctx->j_arc4 = (ctx->j_arc4 + ctx->array_arc4[ctx->i_arc4] + key[ctx->i_arc4 % 256]) % 256;
        tmp = ctx->array_arc4[ctx->i_arc4];
        ctx->array_arc4[ctx->i_arc4] = ctx->array_arc4[ctx->j_arc4];
        ctx->array_arc4[ctx->j_arc4] = tmp;
    }
    ctx->i_arc4 = 0;
    ctx->j_arc4 = 0;
}

/* ARC4 output combined with SplitMix64 stream */
static unsigned char arc4_output(PC4Context *ctx) {
    uint8_t rndbyte, decal;
    int tmp, t;

    ctx->i_arc4 = (ctx->i_arc4 + 1) % 256;
    ctx->j_arc4 = (ctx->j_arc4 + ctx->array_arc4[ctx->i_arc4]) % 256;
    tmp = ctx->array_arc4[ctx->i_arc4];
    ctx->array_arc4[ctx->i_arc4] = ctx->array_arc4[ctx->j_arc4];
    ctx->array_arc4[ctx->j_arc4] = tmp;
    t = (ctx->array_arc4[ctx->i_arc4] + ctx->array_arc4[ctx->j_arc4]) % 256;

    if (ctx->xyz == 0) ctx->bb = next_rng(ctx);
    decal = (uint8_t)(56 - (8 * ctx->xyz));
    rndbyte = (uint8_t)((ctx->bb >> decal) & 0xffu);
    ctx->xyz++;
    if (ctx->xyz == 8) ctx->xyz = 0;

    if (ctx->count == 0) {
        rndbyte = (uint8_t)(rndbyte ^ ctx->array_arc4[t]);
        ctx->count = 1;
    } else {
        rndbyte = (uint8_t)(rndbyte + ctx->array_arc4[t]);
        ctx->count = 0;
    }
    return rndbyte;
}

/* Initialize MD2-II state */
static void md2_init(PC4Context *ctx) {
    ctx->x1 = 0;
    ctx->x2 = 0;
    for (ctx->i = 0; ctx->i < n1; ctx->i++) ctx->h2[ctx->i] = 0;
    for (ctx->i = 0; ctx->i < n1; ctx->i++) ctx->h1[ctx->i] = 0;
}

/* MD2-II hashing */
static void md2_hashing(PC4Context *ctx, unsigned char t1[], size_t b6) {
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
        for (; b6 && ctx->x2 < n1; b6--, ctx->x2++) {
            b5 = t1[b4++];
            ctx->h1[ctx->x2 + n1] = (unsigned char)b5;
            ctx->h1[ctx->x2 + (n1 * 2)] = (unsigned char)(b5 ^ ctx->h1[ctx->x2]);
            ctx->x1 = ctx->h2[ctx->x2] ^= s4[b5 ^ ctx->x1];
        }
        if (ctx->x2 == n1) {
            b2 = 0;
            ctx->x2 = 0;
            for (b3 = 0; b3 < (n1 + 2); b3++) {
                for (b1 = 0; b1 < (n1 * 3); b1++)
                    b2 = ctx->h1[b1] ^= s4[b2];
                b2 = (b2 + b3) % 256;
            }
        }
    }
}

/* Finalize MD2-II */
static void md2_end(PC4Context *ctx, unsigned char h4[n1]) {
    unsigned char h3[n1];
    int i, n4 = n1 - ctx->x2;
    for (i = 0; i < n4; i++) h3[i] = (unsigned char)n4;
    md2_hashing(ctx, h3, (size_t)n4);
    md2_hashing(ctx, ctx->h2, sizeof(ctx->h2));
    for (i = 0; i < n1; i++) h4[i] = ctx->h1[i];
}

/* Generate a random index */
static int mixy(PC4Context *ctx, int nn2) {
    return arc4_output(ctx) % nn2;
}

/* Fisher-Yates shuffle */
static void mixer(PC4Context *ctx, uint8_t *mixu, int nn) {
    int ii, jj, tmmp;
    for (ii = nn - 1; ii > 0; ii--) {
        jj = mixy(ctx, ii + 1);
        tmmp = mixu[jj];
        mixu[jj] = mixu[ii];
        mixu[ii] = (uint8_t)tmmp;
    }
}

/* Key schedule and S-box generation */
void create_keys(PC4Context *ctx, unsigned char key1[], size_t size1) {
    int i, w, k;
    unsigned char h4[n1];

    md2_init(ctx);
    md2_hashing(ctx, key1, size1);
    md2_end(ctx, h4);

    for (i = 0; i < 16; i++) ctx->keys[i] = h4[i];
    arc4_init(ctx, h4);

    ctx->x = 0;
    for (i = 0; i < 8; i++) ctx->x = (ctx->x << 8) + (uint64_t)(h4[256 + i] & 0xffu);

    ctx->xyz = 0;
    ctx->count = 0;

    for (i = 0; i < 20000; i++) (void)arc4_output(ctx);

    uint8_t numbers[256];

    for (w = 0; w < 16; w++) {
        k = arc4_output(ctx) + 256;
        for (i = 0; i < k; i++) (void)arc4_output(ctx);
        for (i = 0; i < 256; i++) numbers[i] = (uint8_t)i;
        mixer(ctx, numbers, 256);
        for (i = 0; i < 256; i++) ctx->perm[w][i] = numbers[i];
    }

    k = arc4_output(ctx) + 256;
    for (i = 0; i < k; i++) (void)arc4_output(ctx);
    for (i = 0; i < 256; i++) numbers[i] = (uint8_t)i;
    mixer(ctx, numbers, 256);
    for (i = 0; i < 256; i++) ctx->new1[i] = numbers[i];

    k = arc4_output(ctx) + 256;
    for (i = 0; i < k; i++) (void)arc4_output(ctx);
    for (i = 0; i < 49; i++) numbers[i] = (uint8_t)i;
    mixer(ctx, numbers, 49);
    for (i = 0; i < 49; i++) ctx->array[i] = numbers[i];

    k = arc4_output(ctx) + 256;
    for (i = 0; i < k; i++) (void)arc4_output(ctx);
    for (i = 0; i < nbround; i++) ctx->decal[i] = (uint8_t)((arc4_output(ctx) % 23) + 1);

    k = arc4_output(ctx) + 256;
    for (i = 0; i < k; i++) (void)arc4_output(ctx);
    for (w = 0; w < 3; w++)
        for (i = 0; i < nbround; i++)
            ctx->rngxor[i][w] = arc4_output(ctx);

    k = arc4_output(ctx) + 256;
    for (i = 0; i < k; i++) (void)arc4_output(ctx);
    for (i = 0; i < 49; i++) numbers[i] = (uint8_t)i;
    mixer(ctx, numbers, 49);
    for (i = 0; i < 49; i++) ctx->array2[i] = numbers[i];

    k = arc4_output(ctx) + 256;
    for (i = 0; i < k; i++) (void)arc4_output(ctx);
    for (i = 0; i < 256; i++) numbers[i] = (uint8_t)i;
    mixer(ctx, numbers, 256);
    for (i = 0; i < 256; i++) {
        ctx->tab[i] = numbers[i];
        ctx->inv[ctx->tab[i]] = (unsigned char)i;
    }

    k = arc4_output(ctx) + 256;
    for (i = 0; i < k; i++) (void)arc4_output(ctx);
    for (w = 0; w < 3; w++) {
        k = arc4_output(ctx) + 256;
        for (i = 0; i < k; i++) (void)arc4_output(ctx);
        for (i = 0; i < 3; i++) numbers[i] = (uint8_t)i;
        mixer(ctx, numbers, 3);
        for (i = 0; i < 3; i++) ctx->permut[w][i] = numbers[i];
    }

    k = arc4_output(ctx) + 256;
    for (i = 0; i < k; i++) (void)arc4_output(ctx);
    for (w = 0; w < 3; w++)
        for (i = 0; i < nbround; i++)
            ctx->rngxor2[i][w] = arc4_output(ctx);
}

/* Compute round transformation */
static void compute(PC4Context *ctx, uint8_t *tab1, uint8_t round) {
    ctx->tot[0] = (uint8_t)((ctx->perm[round][tab1[ctx->permut[0][0]]] +
                             ctx->perm[round][tab1[ctx->permut[0][1]]]) ^
                            ctx->perm[round][tab1[ctx->permut[0][2]]]);
    ctx->tot[0] = (uint8_t)(ctx->tot[0] + ctx->new1[ctx->tot[0]]);
    ctx->tot[1] = (uint8_t)((ctx->perm[round][tab1[ctx->permut[1][0]]] +
                             ctx->perm[round][tab1[ctx->permut[1][1]]]) ^
                            ctx->perm[round][tab1[ctx->permut[1][2]]]);
    ctx->tot[1] = (uint8_t)(ctx->tot[1] + ctx->new1[ctx->tot[1]]);
    ctx->tot[2] = (uint8_t)((ctx->perm[round][tab1[ctx->permut[2][0]]] +
                             ctx->perm[round][tab1[ctx->permut[2][1]]]) ^
                            ctx->perm[round][tab1[ctx->permut[2][2]]]);
    ctx->tot[2] = (uint8_t)(ctx->tot[2] + ctx->new1[ctx->tot[2]]);
}

/* Convert bits to bytes */
void binhex(PC4Context *ctx, short *z, int length) {
    short *b = (short *)z;
    uint8_t i, j;
    for (i = 0; i < length; i = j) {
        uint8_t a = 0;
        for (j = i; j < i + 8; ++j) {
            a |= (uint8_t)(b[((short)(7 - (j % 8)) + j) - (j % 8)] << (j - i));
        }
        ctx->convert[ctx->ptconvert] = a;
        ctx->ptconvert++;
    }
}

/* Convert byte to bits */
void hexbin(PC4Context *ctx, short *q, uint8_t w, uint8_t hex) { // warning: unused parameter ‘ctx’ [-Wunused-parameter]
    short *bits = (short *)q;
    for (uint8_t i = 0; i < 8; ++i) {
        bits[(short)(7 + w) - i] = (short)((hex >> i) & 1u);
    }

    UNUSEDPC4(ctx); //fix above warning
}

/* Encrypt one block */
void pc4encrypt(PC4Context *ctx) {
    int i;
    ctx->totb = 0;

    for (i = 0; i < 3; i++) {
        ctx->l[0][i] = ctx->convert[i];
        ctx->r[0][i] = ctx->convert[i + 3];
    }

    for (i = 1; i <= ctx->rounds; i++) {
        ctx->totb ^= ctx->r[(i - 1) % 2][0];
        ctx->totb ^= ctx->r[(i - 1) % 2][1];
        ctx->totb ^= ctx->r[(i - 1) % 2][2];

        ctx->r[(i - 1) % 2][0] += (uint8_t)(~ctx->rngxor2[ctx->rounds - i][0]);
        ctx->r[(i - 1) % 2][1] ^= (uint8_t)(~ctx->rngxor2[ctx->rounds - i][1]);
        ctx->r[(i - 1) % 2][2] += (uint8_t)(~ctx->rngxor2[ctx->rounds - i][2]);

        ctx->result = 0;
        ctx->result += ((uint32_t)ctx->r[(i - 1) % 2][0] << 16);
        ctx->result += ((uint32_t)ctx->r[(i - 1) % 2][1] << 8);
        ctx->result += ctx->r[(i - 1) % 2][2];

        ctx->result = rol(ctx->result, ctx->decal[i - 1], 24);

        ctx->r[(i - 1) % 2][0] = (uint8_t)(ctx->result >> 16);
        ctx->r[(i - 1) % 2][1] = (uint8_t)((ctx->result >> 8) & 0xffu);
        ctx->r[(i - 1) % 2][2] = (uint8_t)(ctx->result & 0xffu);

        ctx->r[(i - 1) % 2][0] = ctx->tab[ctx->r[(i - 1) % 2][0]];
        ctx->r[(i - 1) % 2][0] ^= ctx->rngxor[i - 1][0];

        ctx->r[(i - 1) % 2][1] = ctx->inv[ctx->r[(i - 1) % 2][1]];
        ctx->r[(i - 1) % 2][1] -= ctx->rngxor[i - 1][1];

        ctx->r[(i - 1) % 2][2] = ctx->tab[ctx->r[(i - 1) % 2][2]];
        ctx->r[(i - 1) % 2][2] ^= ctx->rngxor[i - 1][2];

        compute(ctx, ctx->r[(i - 1) % 2], (uint8_t)((i - 1) % 16));

        ctx->l[i % 2][0] = ctx->r[(i - 1) % 2][0];
        ctx->r[i % 2][0] = ctx->l[(i - 1) % 2][0] - ctx->tot[0];

        ctx->l[i % 2][1] = ctx->r[(i - 1) % 2][1];
        ctx->r[i % 2][1] = ctx->l[(i - 1) % 2][1] ^ ctx->tot[1];

        ctx->l[i % 2][2] = ctx->r[(i - 1) % 2][2];
        ctx->r[i % 2][2] = ctx->l[(i - 1) % 2][2] - ctx->tot[2];
    }

    for (i = 0; i < 3; i++) {
        ctx->convert[i + 3] = ctx->l[(ctx->rounds - 1) % 2][i];
        ctx->convert[i] = ctx->r[(ctx->rounds - 1) % 2][i];
    }

    ctx->totb %= 2;
}

/* Decrypt one block */
void pc4decrypt(PC4Context *ctx) {
    int i;
    ctx->totb = 0;

    for (i = 0; i < 3; i++) {
        ctx->l[0][i] = ctx->convert[i];
        ctx->r[0][i] = ctx->convert[i + 3];
    }

    ctx->y = (uint8_t)((ctx->rounds - 1) % 16);
    if (ctx->y == 0) ctx->y = 16;

    for (i = 1; i <= ctx->rounds; i++) {
        ctx->y--;
        compute(ctx, ctx->r[(i - 1) % 2], ctx->y);
        if (ctx->y == 0) ctx->y = 16;

        ctx->result = 0;

        ctx->l[(i - 1) % 2][0] ^= ctx->rngxor[ctx->rounds - i][0];
        ctx->l[(i - 1) % 2][0] = ctx->inv[ctx->l[(i - 1) % 2][0]];

        ctx->l[(i - 1) % 2][1] += ctx->rngxor[ctx->rounds - i][1];
        ctx->l[(i - 1) % 2][1] = ctx->tab[ctx->l[(i - 1) % 2][1]];

        ctx->l[(i - 1) % 2][2] ^= ctx->rngxor[ctx->rounds - i][2];
        ctx->l[(i - 1) % 2][2] = ctx->inv[ctx->l[(i - 1) % 2][2]];

        ctx->result += ((uint32_t)ctx->l[(i - 1) % 2][0] << 16);
        ctx->result += ((uint32_t)ctx->l[(i - 1) % 2][1] << 8);
        ctx->result += ctx->l[(i - 1) % 2][2];

        ctx->result = ror(ctx->result, ctx->decal[ctx->rounds - i], 24);

        ctx->l[(i - 1) % 2][0] = (uint8_t)(ctx->result >> 16);
        ctx->l[(i - 1) % 2][1] = (uint8_t)((ctx->result >> 8) & 0xffu);
        ctx->l[(i - 1) % 2][2] = (uint8_t)(ctx->result & 0xffu);

        ctx->l[(i - 1) % 2][0] -= (uint8_t)(~ctx->rngxor2[i - 1][0]);
        ctx->l[(i - 1) % 2][1] ^= (uint8_t)(~ctx->rngxor2[i - 1][1]);
        ctx->l[(i - 1) % 2][2] -= (uint8_t)(~ctx->rngxor2[i - 1][2]);

        ctx->totb ^= ctx->l[(i - 1) % 2][0];
        ctx->totb ^= ctx->l[(i - 1) % 2][1];
        ctx->totb ^= ctx->l[(i - 1) % 2][2];

        ctx->l[i % 2][0] = ctx->r[(i - 1) % 2][0];
        ctx->r[i % 2][0] = ctx->l[(i - 1) % 2][0] + ctx->tot[0];

        ctx->l[i % 2][1] = ctx->r[(i - 1) % 2][1];
        ctx->r[i % 2][1] = ctx->l[(i - 1) % 2][1] ^ ctx->tot[1];

        ctx->l[i % 2][2] = ctx->r[(i - 1) % 2][2];
        ctx->r[i % 2][2] = ctx->l[(i - 1) % 2][2] + ctx->tot[2];
    }

    for (i = 0; i < 3; i++) {
        ctx->convert[i + 3] = ctx->l[(ctx->rounds - 1) % 2][i];
        ctx->convert[i] = ctx->r[(ctx->rounds - 1) % 2][i];
    }

    ctx->totb %= 2;
}
