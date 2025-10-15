#include <stdint.h>
#include <stddef.h>

#ifndef BAOFENG_AP
#define BAOFENG_AP

#define nbround 254
#define n1 264

#define UNUSEDPC5(x)                       ((void)x)

/* Structure holding all PC5 state variables */
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
    uint8_t numbers[25];
} PC5Context;

/* Global context accessible from all .c files */
extern PC5Context ctxpc5;

/* Public API */
void create_keys_pc5(PC5Context *ctxpc5, unsigned char key1[], size_t size1);
void pc5encrypt(PC5Context *ctxpc5);
void pc5decrypt(PC5Context *ctxpc5);
void binhexpc5(PC5Context *ctxpc5, short *z, int length);
void hexbinpc5(PC5Context *ctxpc5, short *q, uint8_t w, uint8_t hex);
static void u64_to_bytes_be_pc5(uint64_t val, unsigned char *out);

/* Encrypt a 49-bit frame using PC5 algorithm (original flow) */
static void encrypt_frame_49_pc5(short frame_bits_in[49]) {
    // Only process first 24 bits for encryption, rest are XORed
    ctxpc5.ptconvert = 0;
    binhexpc5(&ctxpc5, frame_bits_in, 24);
    
    uint8_t convert[6];
    
    // Store first 3 bytes in convert
    for (int i = 0; i < 3; i++) convert[i] = ctxpc5.convert[i];
    
    // Split each byte into two 4-bit nibbles
    ctxpc5.convert[0] = convert[0] >> 4;
    ctxpc5.convert[1] = convert[0] & 0xF;
    ctxpc5.convert[2] = convert[1] >> 4;
    ctxpc5.convert[3] = convert[1] & 0xF;
    ctxpc5.convert[4] = convert[2] >> 4;
    ctxpc5.convert[5] = convert[2] & 0xF;
    
    pc5encrypt(&ctxpc5);
    
    // Store encrypted nibbles back in convert
    for (int i = 0; i < 6; i++) convert[i] = ctxpc5.convert[i];
    
    // Recombine nibbles into bytes
    ctxpc5.convert[0] = (convert[0] << 4) + convert[1];
    ctxpc5.convert[1] = (convert[2] << 4) + convert[3];
    ctxpc5.convert[2] = (convert[4] << 4) + convert[5];
    
    // Convert back to bits for first 24 bits
    for (int q = 0; q < 3; q++) {
        uint8_t w = (uint8_t)(q * 8);
        hexbinpc5(&ctxpc5, frame_bits_in, w, ctxpc5.convert[q]);
    }
    
    // XOR the remaining bits (positions 24 to 48)
    for (int i = 24; i < 49; i++) {
        frame_bits_in[i] = (short)(frame_bits_in[i] ^ ctxpc5.numbers[i - 24]);
    }
    
    for (int i = 0; i < 49; i++) ctxpc5.bits[i] = frame_bits_in[i];
}

/* Decrypt a 49-bit frame using PC5 algorithm (original flow) */
static void decrypt_frame_49_pc5(short frame_bits_in[49]) {
    // XOR the remaining bits (positions 24 to 48) first
    for (int i = 24; i < 49; i++) {
        frame_bits_in[i] = (short)(frame_bits_in[i] ^ ctxpc5.numbers[i - 24]);
    }
    
    ctxpc5.ptconvert = 0;
    binhexpc5(&ctxpc5, frame_bits_in, 24);
    
    uint8_t convert[6];
    
    // Store first 3 bytes in convert
    for (int i = 0; i < 3; i++) convert[i] = ctxpc5.convert[i];

    // Split each byte into two 4-bit nibbles
    ctxpc5.convert[0] = convert[0] >> 4;
    ctxpc5.convert[1] = convert[0] & 0xF;
    ctxpc5.convert[2] = convert[1] >> 4;
    ctxpc5.convert[3] = convert[1] & 0xF;
    ctxpc5.convert[4] = convert[2] >> 4;
    ctxpc5.convert[5] = convert[2] & 0xF;
    
    pc5decrypt(&ctxpc5);
    
    // Store decrypted nibbles back in convert
    for (int i = 0; i < 6; i++) convert[i] = ctxpc5.convert[i];
    
    // Recombine nibbles into bytes
    ctxpc5.convert[0] = (convert[0] << 4) + convert[1];
    ctxpc5.convert[1] = (convert[2] << 4) + convert[3];
    ctxpc5.convert[2] = (convert[4] << 4) + convert[5];
    
    // Convert back to bits for first 24 bits
    for (int q = 0; q < 3; q++) {
        uint8_t w = (uint8_t)(q * 8);
        hexbinpc5(&ctxpc5, frame_bits_in, w, ctxpc5.convert[q]);
    }
    
     for (int i = 0; i < 49; i++) ctxpc5.bits[i] = frame_bits_in[i];
}

/* Convert 64-bit integer to bytes (big-endian) */
static void u64_to_bytes_be_pc5(uint64_t val, unsigned char *out) {
    for (int i = 0; i < 8; i++) {
        out[i] = (unsigned char)((val >> (56 - 8 * i)) & 0xFF);
    }
}

#endif
