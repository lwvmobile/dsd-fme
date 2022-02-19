/*
 * Copyright (C) 2010 DSD Author
 * GPG Key ID: 0x3F1D7FD0 (74EF 430D F7F2 0A48 FCE6  F630 FAA2 635D 3F1D 7FD0)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _MAIN
extern const int nW[36];
extern const int nX[36];
extern const int nY[36];
extern const int nZ[36];
extern const char nxdnpr[145];
extern const char nxdnpr2[145]; //why is this 145 but referenced later as 182 or 384
//borrow from LEH
extern const unsigned char nsW[72];
extern const int DSDNXDN_SACCH_m_Interleave[60];
extern const int DSDNXDN_SACCH_m_PunctureList[12];
extern const int DSDNXDN_FACCH1_m_Interleave[144];
extern const int DSDNXDN_FACCH1_m_PunctureList[48];
extern const int DSDNXDN_UDCH_m_Interleave[348];
extern const int DSDNXDN_UDCH_m_PunctureList[58];
//
//enum
//{
#define   NXDN_VCALL           0b000001  /* VCALL = VCALL_REQ = VCALL_RESP */
#define   NXDN_VCALL_IV        0b000011
#define   NXDN_DCALL_HDR       0b001001  /* Header Format - NXDN_DCALL_HDR = NXDN_DCALL_REQ = NXDN_DCALL_RESP */
#define   NXDN_DCALL_USR       0b001011  /* User Data Format */
#define   NXDN_DCALL_ACK       0b001100
#define   NXDN_REL_EX          0b000111
#define   NXDN_HEAD_DLY        0b001111
#define   NXDN_SDCALL_REQ_HDR  0b111000  /* Header Format */
#define   NXDN_SDCALL_REQ_USR  0b111001  /* User Data Format */
#define   NXDN_SDCALL_RESP     0b111011
#define   NXDN_SDCALL_IV       0b111010
#define   NXDN_STAT_INQ_REQ    0b110000
#define   NXDN_STAT_INQ_RESP   0b110001
#define   NXDN_STAT_REQ        0b110010
#define   NXDN_STAT_RESP       0b110011
#define   NXDN_REM_CON_REQ     0b110100
#define   NXDN_REM_CON_RESP    0b110101
#define   NXDN_REM_CON_E_REQ   0b110110
#define   NXDN_REM_CON_E_RESP  0b110111
//NXDN_VCALL_REQ       = 0b000001,  /* VCALL = VCALL_REQ = VCALL_RESP */
//NXDN_VCALL_RESP      = 0b000001,  /* VCALL = VCALL_REQ = VCALL_RESP */
#define   NXDN_VCALL_REC_REQ   = 0b000010  /* NXDN_VCALL_REC_REQ = NXDN_VCALL_REC_RESP */
//NXDN_VCALL_REC_RESP  = 0b000010,
#define   NXDN_VCALL_CONN_REQ  = 0b000011  /* NXDN_VCALL_CONN_REQ = NXDN_VCALL_CONN_RESP */
//NXDN_VCALL_CONN_RESP = 0b000011,  /* NXDN_VCALL_CONN_REQ = NXDN_VCALL_CONN_RESP */
#define   NXDN_VCALL_ASSGN_DUP = 0b000101
//NXDN_DCALL_REQ       = 0b001001,  /* NXDN_DCALL_HDR = NXDN_DCALL_REQ = NXDN_DCALL_RESP */
//NXDN_DCALL_RESP      = 0b001001,  /* NXDN_DCALL_HDR = NXDN_DCALL_REQ = NXDN_DCALL_RESP */
#define   NXDN_DCALL_REC_REQ   = 0b001010  /* NXDN_DCALL_REC_REQ = NXDN_DCALL_REC_RESP */
//NXDN_DCALL_REC_RESP  = 0b001010,  /* NXDN_DCALL_REC_REQ = NXDN_DCALL_REC_RESP */
#define   NXDN_DCALL_ASSGN     = 0b001110
#define   NXDN_DCALL_ASSGN_DUP = 0b001101
#define   NXDN_IDLE            = 0b010000
#define   NXDN_DISC_REQ        = 0b010001  /* NXDN_DISC_REQ = NXDN_DISC */
#define   NXDN_DISC            = 0b010001   /* NXDN_DISC_REQ = NXDN_DISC */


#else
/*
 *  pseudorandom bit sequence
 */
const char nxdnpr[145] = { 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1 };
//const unsigned char nxdnpr2[384] =
const unsigned char nxdnpr2[182] = //is this a problem, should it be 384/2 for dibits?
{
  0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0,
  1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 1, 0,
  0, 1, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1,
  0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1,
  1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1,
  0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1,
  1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0,
  0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1,
  0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1,
  1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1, 1,
  1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1,
  1, 1, 0, 0, 0, 1//, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1,
//  0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0,
//  1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0,
//  1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
//  0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1,
//  1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0,
//  0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 0, 0, 1,
//  0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1,
//  0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1,
//  1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0,
//  1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0,
//  1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
//  1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1
};
/*
 * NXDN AMBE interleave schedule
 */
const int nW[36] = { 0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 2,
  0, 2, 0, 2, 0, 2,
  0, 2, 0, 2, 0, 2
};

const int nX[36] = { 23, 10, 22, 9, 21, 8,
  20, 7, 19, 6, 18, 5,
  17, 4, 16, 3, 15, 2,
  14, 1, 13, 0, 12, 10,
  11, 9, 10, 8, 9, 7,
  8, 6, 7, 5, 6, 4
};

const int nY[36] = { 0, 2, 0, 2, 0, 2,
  0, 2, 0, 3, 0, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3
};

const int nZ[36] = { 5, 3, 4, 2, 3, 1,
  2, 0, 1, 13, 0, 12,
  22, 11, 21, 10, 20, 9,
  19, 8, 18, 7, 17, 6,
  16, 5, 15, 4, 14, 3,
  13, 2, 12, 1, 11, 0
};

/*
 * NXDN SACCH interleave
 */
const unsigned char nsW[72] = {
  0, 12, 24, 36, 48, 60,
  1, 13, 25, 37, 49, 61,
  2, 14, 26, 38, 50, 62,
  3, 15, 27, 39, 51, 63,
  4, 16, 28, 40, 52, 64,
  5, 17, 29, 41, 53, 65,
  6, 18, 30, 42, 54, 66,
  7, 19, 31, 43, 55, 67,
  8, 20, 32, 44, 56, 68,
  9, 21, 33, 45, 57, 69,
  10,22, 34, 46, 58, 70,
  11,23, 35, 47, 59, 71
};

const int DSDNXDN_SACCH_m_Interleave[60] =
{
  0, 12, 24, 36, 48,
  1, 13, 25, 37, 49,
  2, 14, 26, 38, 50,
  3, 15, 27, 39, 51,
  4, 16, 28, 40, 52,
  5, 17, 29, 41, 53,
  6, 18, 30, 42, 54,
  7, 19, 31, 43, 55,
  8, 20, 32, 44, 56,
  9, 21, 33, 45, 57,
  10, 22, 34, 46, 58,
  11, 23, 35, 47, 59,
};

const int DSDNXDN_SACCH_m_PunctureList[12] = { 5, 11, 17, 23, 29, 35, 41, 47, 53, 59, 65, 71 };

const unsigned int DSDNXDN_FACCH1_m_Interleave[144] =
{
  0,  16, 32, 48, 64, 80, 96,  112, 128,
  1,  17, 33, 49, 65, 81, 97,  113, 129,
  2,  18, 34, 50, 66, 82, 98,  114, 130,
  3,  19, 35, 51, 67, 83, 99,  115, 131,
  4,  20, 36, 52, 68, 84, 100, 116, 132,
  5,  21, 37, 53, 69, 85, 101, 117, 133,
  6,  22, 38, 54, 70, 86, 102, 118, 134,
  7,  23, 39, 55, 71, 87, 103, 119, 135,
  8,  24, 40, 56, 72, 88, 104, 120, 136,
  9,  25, 41, 57, 73, 89, 105, 121, 137,
  10, 26, 42, 58, 74, 90, 106, 122, 138,
  11, 27, 43, 59, 75, 91, 107, 123, 139,
  12, 28, 44, 60, 76, 92, 108, 124, 140,
  13, 29, 45, 61, 77, 93, 109, 125, 141,
  14, 30, 46, 62, 78, 94, 110, 126, 142,
  15, 31, 47, 63, 79, 95, 111, 127, 143,
};

const unsigned int DSDNXDN_FACCH1_m_PunctureList[48] =
{
    1U,   5U,   9U,  13U,  17U,  21U,  25U,  29U,  33U,  37U,
   41U,  45U,  49U,  53U,  57U,  61U,  65U,  69U,  73U,  77U,
   81U,  85U,  89U,  93U,  97U, 101U, 105U, 109U, 113U, 117U,
  121U, 125U, 129U, 133U, 137U, 141U, 145U, 149U, 153U, 157U,
  161U, 165U, 169U, 173U, 177U, 181U, 185U, 189U
};

const int DSDNXDN_UDCH_m_Interleave[348] = {
    0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 120, 132, 144, 156, 168, 180, 192, 204, 216, 228, 240, 252, 264, 276, 288, 300, 312, 324, 336,
    1, 13, 25, 37, 49, 61, 73, 85, 97, 109, 121, 133, 145, 157, 169, 181, 193, 205, 217, 229, 241, 253, 265, 277, 289, 301, 313, 325, 337,
    2, 14, 26, 38, 50, 62, 74, 86, 98, 110, 122, 134, 146, 158, 170, 182, 194, 206, 218, 230, 242, 254, 266, 278, 290, 302, 314, 326, 338,
    3, 15, 27, 39, 51, 63, 75, 87, 99, 111, 123, 135, 147, 159, 171, 183, 195, 207, 219, 231, 243, 255, 267, 279, 291, 303, 315, 327, 339,
    4, 16, 28, 40, 52, 64, 76, 88, 100, 112, 124, 136, 148, 160, 172, 184, 196, 208, 220, 232, 244, 256, 268, 280, 292, 304, 316, 328, 340,
    5, 17, 29, 41, 53, 65, 77, 89, 101, 113, 125, 137, 149, 161, 173, 185, 197, 209, 221, 233, 245, 257, 269, 281, 293, 305, 317, 329, 341,
    6, 18, 30, 42, 54, 66, 78, 90, 102, 114, 126, 138, 150, 162, 174, 186, 198, 210, 222, 234, 246, 258, 270, 282, 294, 306, 318, 330, 342,
    7, 19, 31, 43, 55, 67, 79, 91, 103, 115, 127, 139, 151, 163, 175, 187, 199, 211, 223, 235, 247, 259, 271, 283, 295, 307, 319, 331, 343,
    8, 20, 32, 44, 56, 68, 80, 92, 104, 116, 128, 140, 152, 164, 176, 188, 200, 212, 224, 236, 248, 260, 272, 284, 296, 308, 320, 332, 344,
    9, 21, 33, 45, 57, 69, 81, 93, 105, 117, 129, 141, 153, 165, 177, 189, 201, 213, 225, 237, 249, 261, 273, 285, 297, 309, 321, 333, 345,
    10, 22, 34, 46, 58, 70, 82, 94, 106, 118, 130, 142, 154, 166, 178, 190, 202, 214, 226, 238, 250, 262, 274, 286, 298, 310, 322, 334, 346,
    11, 23, 35, 47, 59, 71, 83, 95, 107, 119, 131, 143, 155, 167, 179, 191, 203, 215, 227, 239, 251, 263, 275, 287, 299, 311, 323, 335, 347,
};

const int DSDNXDN_UDCH_m_PunctureList[58] = {
    3, 11,
    17, 25,
    31, 39,
    45, 53,
    59, 67,
    73, 81,
    87, 95,
    101, 109,
    115, 123,
    129, 137,
    143, 151,
    157, 165,
    171, 179,
    185, 193,
    199, 207,
    213, 221,
    227, 235,
    241, 249,
    255, 263,
    269, 277,
    283, 291,
    297, 305,
    311, 319,
    325, 333,
    339, 347,
    353, 361,
    367, 375,
    381, 389,
    395, 403,
};

//#endif /* _MAIN */



#endif
