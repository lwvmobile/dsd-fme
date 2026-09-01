/*
 * RS(16,13) encoder
 *
 * 13 data bytes + 3 parity bytes = 16-byte codeword
 *
 * Uses the same generator polynomial and GF(256) tables as the
 * DMR RS(12,9) implementation (DMR AI Spec / dmrshark / dsd):
 *
 *   g(x) = x^3 + 0x40 x^2 + 0x38 x + 0x0e
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "dsd.h"

/* -----------------------------------------------------------------------
 * Constants
 * ----------------------------------------------------------------------- */
#define RS_DATASIZE      13
#define RS_CHECKSUMSIZE  3
#define RS_CODEWORDSIZE  (RS_DATASIZE + RS_CHECKSUMSIZE)   /* 16 */

/* -----------------------------------------------------------------------
 * Galois Field tables (GF(256) – same as DMR RS(12,9))
 * ----------------------------------------------------------------------- */
static const uint8_t galois_exp_table[256] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1D, 0x3A, 0x74, 0xE8, 0xCD, 0x87, 0x13, 0x26,
    0x4C, 0x98, 0x2D, 0x5A, 0xB4, 0x75, 0xEA, 0xC9, 0x8F, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0,
    0x9D, 0x27, 0x4E, 0x9C, 0x25, 0x4A, 0x94, 0x35, 0x6A, 0xD4, 0xB5, 0x77, 0xEE, 0xC1, 0x9F, 0x23,
    0x46, 0x8C, 0x05, 0x0A, 0x14, 0x28, 0x50, 0xA0, 0x5D, 0xBA, 0x69, 0xD2, 0xB9, 0x6F, 0xDE, 0xA1,
    0x5F, 0xBE, 0x61, 0xC2, 0x99, 0x2F, 0x5E, 0xBC, 0x65, 0xCA, 0x89, 0x0F, 0x1E, 0x3C, 0x78, 0xF0,
    0xFD, 0xE7, 0xD3, 0xBB, 0x6B, 0xD6, 0xB1, 0x7F, 0xFE, 0xE1, 0xDF, 0xA3, 0x5B, 0xB6, 0x71, 0xE2,
    0xD9, 0xAF, 0x43, 0x86, 0x11, 0x22, 0x44, 0x88, 0x0D, 0x1A, 0x34, 0x68, 0xD0, 0xBD, 0x67, 0xCE,
    0x81, 0x1F, 0x3E, 0x7C, 0xF8, 0xED, 0xC7, 0x93, 0x3B, 0x76, 0xEC, 0xC5, 0x97, 0x33, 0x66, 0xCC,
    0x85, 0x17, 0x2E, 0x5C, 0xB8, 0x6D, 0xDA, 0xA9, 0x4F, 0x9E, 0x21, 0x42, 0x84, 0x15, 0x2A, 0x54,
    0xA8, 0x4D, 0x9A, 0x29, 0x52, 0xA4, 0x55, 0xAA, 0x49, 0x92, 0x39, 0x72, 0xE4, 0xD5, 0xB7, 0x73,
    0xE6, 0xD1, 0xBF, 0x63, 0xC6, 0x91, 0x3F, 0x7E, 0xFC, 0xE5, 0xD7, 0xB3, 0x7B, 0xF6, 0xF1, 0xFF,
    0xE3, 0xDB, 0xAB, 0x4B, 0x96, 0x31, 0x62, 0xC4, 0x95, 0x37, 0x6E, 0xDC, 0xA5, 0x57, 0xAE, 0x41,
    0x82, 0x19, 0x32, 0x64, 0xC8, 0x8D, 0x07, 0x0E, 0x1C, 0x38, 0x70, 0xE0, 0xDD, 0xA7, 0x53, 0xA6,
    0x51, 0xA2, 0x59, 0xB2, 0x79, 0xF2, 0xF9, 0xEF, 0xC3, 0x9B, 0x2B, 0x56, 0xAC, 0x45, 0x8A, 0x09,
    0x12, 0x24, 0x48, 0x90, 0x3D, 0x7A, 0xF4, 0xF5, 0xF7, 0xF3, 0xFB, 0xEB, 0xCB, 0x8B, 0x0B, 0x16,
    0x2C, 0x58, 0xB0, 0x7D, 0xFA, 0xE9, 0xCF, 0x83, 0x1B, 0x36, 0x6C, 0xD8, 0xAD, 0x47, 0x8E, 0x01
};

static const uint8_t galois_log_table[256] = {
    0,   0,   1,  25,   2,  50,  26, 198,   3, 223,  51, 238,  27, 104, 199,  75,
    4, 100, 224,  14,  52, 141, 239, 129,  28, 193, 105, 248, 200,   8,  76, 113,
    5, 138, 101,  47, 225,  36,  15,  33,  53, 147, 142, 218, 240,  18, 130,  69,
   29, 181, 194, 125, 106,  39, 249, 185, 201, 154,   9, 120,  77, 228, 114, 166,
    6, 191, 139,  98, 102, 221,  48, 253, 226, 152,  37, 179,  16, 145,  34, 136,
   54, 208, 148, 206, 143, 150, 219, 189, 241, 210,  19,  92, 131,  56,  70,  64,
   30,  66, 182, 163, 195,  72, 126, 110, 107,  58,  40,  84, 250, 133, 186,  61,
  202,  94, 155, 159,  10,  21, 121,  43,  78, 212, 229, 172, 115, 243, 167,  87,
    7, 112, 192, 247, 140, 128,  99,  13, 103,  74, 222, 237,  49, 197, 254,  24,
  227, 165, 153, 119,  38, 184, 180, 124,  17,  68, 146, 217,  35,  32, 137,  46,
   55,  63, 209,  91, 149, 188, 207, 205, 144, 135, 151, 178, 220, 252, 190,  97,
  242,  86, 211, 171,  20,  42,  93, 158, 132,  60,  57,  83,  71, 109,  65, 162,
   31,  45,  67, 216, 183, 123, 164, 118, 196,  23,  73, 236, 127,  12, 111, 246,
  108, 161,  59,  82,  41, 157,  85, 170, 251,  96, 134, 177, 187, 204,  62,  90,
  203,  89,  95, 176, 156, 169, 160,  81,  11, 245,  22, 235, 122, 117,  44, 215,
   79, 174, 213, 233, 230, 231, 173, 232, 116, 214, 244, 234, 168,  80,  88, 175
};

/* -----------------------------------------------------------------------
 * Galois multiplication
 * ----------------------------------------------------------------------- */
static uint8_t galois_mult(uint8_t a, uint8_t b)
{
    if (a == 0 || b == 0)
        return 0;
    return galois_exp_table[(galois_log_table[a] + galois_log_table[b]) % 255];
}

/* -----------------------------------------------------------------------
 * RS(16,13) encoder – LFSR with the DMR generator polynomial
 *
 * genpoly coefficients (same as RS(12,9)):
 *   { 0x40, 0x38, 0x0e, 0x01 }
 * ----------------------------------------------------------------------- */
void rs16_13_encode(const uint8_t data_in[RS_DATASIZE], uint8_t codeword[RS_CODEWORDSIZE])
{
    /* Generator polynomial coefficients from DMR AI Spec */
    static const uint8_t genpoly[] = { 0x40, 0x38, 0x0e, 0x01 };

    uint8_t parity[3] = { 0, 0, 0 };
    uint8_t i;
    uint8_t feedback;

    for (i = 0; i < RS_DATASIZE; i++) {
        feedback = data_in[i] ^ parity[0];

        parity[0] = parity[1] ^ galois_mult(genpoly[2], feedback);
        parity[1] = parity[2] ^ galois_mult(genpoly[1], feedback);
        parity[2] = galois_mult(genpoly[0], feedback);
    }

    /* Build systematic codeword: data || parity */
    memcpy(codeword, data_in, RS_DATASIZE);
    codeword[13] = parity[0];
    codeword[14] = parity[1];
    codeword[15] = parity[2];
}

//This will run a RS 16,13 encode on 13 bytes of input
//input is an array of 16 bytes or more
void rs_16_13_encode(uint8_t * input)
{

  uint8_t output[16];
  memset(output, 0, sizeof(output));

  //Calculate the 3 parity / checksum bytes
  rs16_13_encode(input, output);

  //append the encoded parity to the input
  input[13] = output[13];
  input[14] = output[14];
  input[15] = output[15];
}
