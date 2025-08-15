#include <stdint.h>
#include <stddef.h>

#ifndef TYT_AP
#define TYT_AP

#define nbround 254
#define n1 264

#define UNUSEDPC4(x)                       ((void)x)

/* Structure holding all PC4 state variables */
typedef struct {
    short bits[49], temp[49];
    uint8_t ptconvert;
    uint8_t convert[7];
    uint8_t perm[16][256];
    uint8_t new1[256];
    uint8_t array[49];
    uint8_t array2[49];
    uint8_t decal[nbround];
    uint8_t rngxor[nbround][3];
    uint8_t rngxor2[nbround][3];
    uint8_t rounds;
    uint8_t tab[256];
    uint8_t inv[256];
    uint8_t permut[3][3];
    uint64_t bb;
    uint64_t x;
    uint8_t tot[3];
    uint8_t l[2][3], r[2][3];
    uint8_t y, totb;
    uint32_t result;
    uint8_t xyz, count;
    uint8_t keys[16];
    unsigned char array_arc4[256];
    int i_arc4, j_arc4;
    int x1, x2, i;
    unsigned char h2[n1];
    unsigned char h1[n1*3];
} PC4Context;

/* Global context accessible from all .c files */
extern PC4Context ctx;

/* Public API */
void create_keys(PC4Context *ctx, unsigned char key1[], size_t size1);
void pc4encrypt(PC4Context *ctx);
void pc4decrypt(PC4Context *ctx);
void binhex(PC4Context *ctx, short *z, int length);
void hexbin(PC4Context *ctx, short *q, uint8_t w, uint8_t hex);
static void u64_to_bytes_be(uint64_t val, unsigned char *out);

/* Encrypt a 49-bit frame (original flow) */
static void encrypt_frame_49(short frame_bits_in[49]) {
    for (int i = 0; i < 49; i++) ctx.bits[i] = frame_bits_in[i];
    for (int i = 0; i < 49; i++) ctx.temp[i] = ctx.bits[ctx.array[i]];
    for (int i = 0; i < 49; i++) ctx.bits[i] = ctx.temp[i];
    ctx.ptconvert = 0;
    binhex(&ctx, ctx.bits, 48);
    pc4encrypt(&ctx);
    for (int q = 0; q < 6; q++) {
        uint8_t w = (uint8_t)(q * 8);
        hexbin(&ctx, ctx.bits, w, ctx.convert[q]);
    }
    ctx.bits[48] = (short)(ctx.bits[48] ^ ctx.totb);
    for (int i = 0; i < 49; i++) ctx.temp[ctx.array2[i]] = ctx.bits[i];
    for (int i = 0; i < 49; i++) ctx.bits[i] = ctx.temp[i];
}

/* Decrypt a 49-bit frame (original flow) */
static void decrypt_frame_49(short frame_bits_in[49]) {
    for (int i = 0; i < 49; i++) ctx.bits[i] = frame_bits_in[i];
    for (int i = 0; i < 49; i++) ctx.temp[i] = ctx.bits[ctx.array2[i]];
    for (int i = 0; i < 49; i++) ctx.bits[i] = ctx.temp[i];
    ctx.ptconvert = 0;
    binhex(&ctx, ctx.bits, 48);
    pc4decrypt(&ctx);
    for (int q = 0; q < 6; q++) {
        uint8_t w = (uint8_t)(q * 8);
        hexbin(&ctx, ctx.bits, w, ctx.convert[q]);
    }
    ctx.bits[48] = (short)(ctx.bits[48] ^ ctx.totb);
    for (int i = 0; i < 49; i++) ctx.temp[ctx.array[i]] = ctx.bits[i];
    for (int i = 0; i < 49; i++) ctx.bits[i] = ctx.temp[i];
}

/* Convert 64-bit integer to bytes (big-endian) */
static void u64_to_bytes_be(uint64_t val, unsigned char *out) {
    for (int i = 0; i < 8; i++) {
        out[i] = (unsigned char)((val >> (56 - 8 * i)) & 0xFF);
    }
}

#endif
