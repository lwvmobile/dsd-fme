/*-------------------------------------------------------------------------------
 * ysf.c
 * Yaesu Fusion Decoder (WIP)
 *
 * Bits of code and ideas from DSDcc, Osmocom OP25, gr-ysf, Munaut sprinkled in
 *
 * LWVMOBILE
 * 2023-07 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/
#include "dsd.h"

/* thx gr-ysf fr_vch_decoder_bb_impl.cc * Copyright 2015 Mathias Weyland */
// I hold Sylvain Munaut in high esteem for figuring this out.
uint8_t fr_interleave[144] = {
  0,   7,  12,  19,  24,  31,  36,  43,  48,  55,  60,  67,     // [  0 -  11] yellow message
 72,  79,  84,  91,  96, 103, 108, 115, 120, 127, 132,          // [ 12 -  22] yellow FEC
139,   1,   6,  13,  18,  25,  30,  37,  42,  49,  54,  61,     // [ 23 -  34] orange message
 66,  73,  78,  85,  90,  97, 102, 109, 114, 121, 126,          // [ 35 -  45] orange FEC
133, 138,   2,   9,  14,  21,  26,  33,  38,  45,  50,  57,     // [ 46 -  57] red message
 62,  69,  74,  81,  86,  93,  98, 105, 110, 117, 122,          // [ 58 -  68] red FEC
129, 134, 141,   3,   8,  15,  20,  27,  32,  39,  44,  51,     // [ 69 -  80] pink message
 56,  63,  68,  75,  80,  87,  92,  99, 104, 111, 116,          // [ 81 -  91] pink FEC
123, 128, 135, 140,   4,  11,  16,  23,  28,  35,  40,          // [ 92 - 102] dark blue message
 47,  52,  59,  64,                                             // [103 - 106] dark blue FEC
 71,  76,  83,  88,  95, 100, 107, 112, 119, 124, 131,          // [107 - 117] light blue message
136, 143,   5,  10,                                             // [118 - 121] light blue FEC
 17,  22,  29,  34,  41,  46,  53,  58,  65,  70,  77,          // [122 - 132] green message
 82,  89,  94, 101,                                             // [133 - 136] green FEC
106, 113, 118, 125, 130, 137, 142,                              // [137 - 143] unprotected
};

uint8_t pn95[512] =
{
  1, 0, 0, 1, 0, 0, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1,
  0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 
  1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 
  0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 
  1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 
  1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 1, 
  0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 
  1, 1, 1, 0, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 
  0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 
  1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 0, 
  0, 1, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 
  0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 
  0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 
  1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 
  0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 
  0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 
  1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 
  0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 
  0, 0, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 
  0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 
  0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 
  1, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 
  1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 
  0, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 
  0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 
  1, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 
  1, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 
  1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 
  0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 
  0, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 
  0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 1
};

//half-rate (from NXDN)
const int YnW[36] = 
{ 0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 2,
  0, 2, 0, 2, 0, 2,
  0, 2, 0, 2, 0, 2
};

const int YnX[36] = 
{ 23, 10, 22, 9, 21, 8,
  20, 7, 19, 6, 18, 5,
  17, 4, 16, 3, 15, 2,
  14, 1, 13, 0, 12, 10,
  11, 9, 10, 8, 9, 7,
  8, 6, 7, 5, 6, 4
};

const int YnY[36] = 
{ 0, 2, 0, 2, 0, 2,
  0, 2, 0, 3, 0, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3
};

const int YnZ[36] = 
{ 5, 3, 4, 2, 3, 1,
  2, 0, 1, 13, 0, 12,
  22, 11, 21, 10, 20, 9,
  19, 8, 18, 7, 17, 6,
  16, 5, 15, 4, 14, 3,
  13, 2, 12, 1, 11, 0
};

//M = 26, depth of 4; -- from DSDcc
const int vd2Interleave[104] = {
0,  26,  52,  78,
1,  27,  53,  79,
2,  28,  54,  80,
3,  29,  55,  81,
4,  30,  56,  82,
5,  31,  57,  83,
6,  32,  58,  84,
7,  33,  59,  85,
8,  34,  60,  86,
9,  35,  61,  87,
10,  36,  62,  88,
11,  37,  63,  89,
12,  38,  64,  90,
13,  39,  65,  91,
14,  40,  66,  92,
15,  41,  67,  93,
16,  42,  68,  94,
17,  43,  69,  95,
18,  44,  70,  96,
19,  45,  71,  97,
20,  46,  72,  98,
21,  47,  73,  99,
22,  48,  74, 100,
23,  49,  75, 101,
24,  50,  76, 102,
25,  51,  77, 103
};

void ysf_dch_decode (dsd_state * state, uint8_t bn, uint8_t bt, uint8_t fn, uint8_t ft, uint8_t cm, uint8_t input[])
{
  //TODO: Per Call WAV files using these strings
  int i;
  char dch_bytes[20];
  memset (dch_bytes, 0, sizeof(dch_bytes));
  char string1[11];
  char string2[11];
  char rem1[6];
  char rem2[6];

  UNUSED3(bt, fn, ft);

  for (i = 0; i < 20; i++)
    dch_bytes[i] = (char)ConvertBitIntoBytes(&input[i*8], 8);

  switch(bn){ //using bn here so we can use the frame number for sorting the text messages found in here
    case 0: //CSD1

      //Destination / Target
      if (cm != 1)
      {
        memcpy (string1, dch_bytes, 10);
        string1[10] = '\0';
        fprintf (stderr, "DST: ");
        fprintf (stderr, "%s ", string1);
      }
      else //Radio ID Mode -- updated in the 1V02 spec manual
      {
        memcpy (rem1, dch_bytes, 5);
        rem1[5] = '\0';
        fprintf (stderr, "DST RID: ");
        fprintf (stderr, "%s ", rem1);

        memcpy (rem2, dch_bytes+5, 5);
        rem2[5] = '\0';
        fprintf (stderr, "SRC RID: ");
        fprintf (stderr, "%s ", rem2);
      }

      //Source
      memcpy (string2, dch_bytes+10, 10);
      string2[10] = '\0';
      fprintf (stderr, "SRC: ");
      fprintf (stderr, "%s ", string2);

      //Copy both to Ncurses Call String
      memcpy (state->ysf_tgt, dch_bytes, 10);
      state->ysf_tgt[10] = '\0';

      memcpy (state->ysf_src, dch_bytes+10, 10);
      state->ysf_src[10] = '\0';

      break;
    case 1: //CSD2

      //Uplink
      memcpy (string1, dch_bytes, 10);
      string1[10] = '\0';
      fprintf (stderr, "U/L: ");
      fprintf (stderr, "%s ", string1);

      //Downlink
      memcpy (string2, dch_bytes+10, 10);
      string2[10] = '\0';
      fprintf (stderr, "D/L: ");
      fprintf (stderr, "%s ", string2);
      
      //Copy both to Ncurses Call String
      memcpy (state->ysf_upl, dch_bytes, 10);
      state->ysf_upl[10] = '\0';

      state->ysf_dnl[10] = '\0';
      memcpy (state->ysf_dnl, dch_bytes+10, 10);

      break;
    case 2:
      for (i = 0; i < 20; i++)
      {
        if (dch_bytes[i] > 0x19 && dch_bytes[i] < 0x7F)
          fprintf (stderr, "%c", dch_bytes[i]);
        else fprintf (stderr, ".");
      }
      fprintf (stderr, " ");

      // if (fn == 0) memset (state->ysf_txt, 0, sizeof(state->ysf_txt));
      //copy text to txt storage -- works now (had to expand storage space), but is cumbersome in ncurses (too long)
      // if (fn < 20)
      // {
      //   //switch to checking each byte for a 'nice' ASCII character and 
      //   //not a 'naughty' del/rem/break/garbled non ASCII/ALPHANUMERIC type character  
      //   for (i = 0; i < 20; i++)
      //   {
      //     C = dch_bytes[i]; //skipping 0x20 space key
      //     if (C > 0x20 && C < 0x7F) state->ysf_txt[fn][i] = C;
      //     else state->ysf_txt[fn][i] = 0; //NULL
      //   }
      // }
        
      break;
      
    default:
      break;

  }

}

void ysf_dch_decode2 (dsd_state * state, uint8_t bn, uint8_t bt, uint8_t fn, uint8_t ft, uint8_t cm, uint8_t input[])
{
  //TODO: Per Call WAV files using these strings
  int i;
  char dch_bytes[20];
  memset (dch_bytes, 0, sizeof(dch_bytes));
  char string[11];
  char rem1[6];
  char rem2[6];

  UNUSED4(bn, bt, fn, ft);

  for (i = 0; i < 10; i++)
    dch_bytes[i] = (char)ConvertBitIntoBytes(&input[i*8], 8);

  switch(fn){
    case 0:
      //Destination / Target
      if (cm != 1)
      {
        memcpy (string, dch_bytes, 10);
        string[10] = '\0';
        fprintf (stderr, "DST: ");
        fprintf (stderr, "%s ", string);
      }
      else //Radio ID Mode -- updated in the 1V02 spec manual
      {
        memcpy (rem1, dch_bytes, 5);
        rem1[5] = '\0';
        fprintf (stderr, "DST RID: ");
        fprintf (stderr, "%s ", rem1);

        memcpy (rem2, dch_bytes+5, 5);
        rem2[5] = '\0';
        fprintf (stderr, "SRC RID: ");
        fprintf (stderr, "%s ", rem2);
      }

      memcpy (state->ysf_tgt, dch_bytes, 10);
      state->ysf_tgt[10] = '\0';
      

      break;
    case 1:
      //Source
      memcpy (string, dch_bytes, 10);
      string[10] = '\0';
      fprintf (stderr, "SRC: ");
      fprintf (stderr, "%s", string);

      memcpy (state->ysf_src, dch_bytes, 10);
      state->ysf_src[10] = '\0';

      break;
    case 2:
      //Uplink
      memcpy (string, dch_bytes, 10);
      string[10] = '\0';
      fprintf (stderr, "U/L: ");
      fprintf (stderr, "%s", string);

      memcpy (state->ysf_upl, dch_bytes, 10);
      state->ysf_upl[10] = '\0';

      break;
    case 3:
      //Downlink
      memcpy (string, dch_bytes, 10);
      string[10] = '\0';
      fprintf (stderr, "D/L: ");
      fprintf (stderr, "%s", string);

      state->ysf_dnl[10] = '\0';
      memcpy (state->ysf_dnl, dch_bytes, 10);

      break;
    case 4:
      //Remarks 1 and 2
      memcpy (rem1, dch_bytes, 5);
      rem1[5] = '\0';
      fprintf (stderr, "RM1: ");
      fprintf (stderr, "%s ", rem1);
      
      memcpy (rem2, dch_bytes+5, 5);
      rem2[5] = '\0';
      fprintf (stderr, "RM2: ");
      fprintf (stderr, "%s ", rem2);

      memcpy (state->ysf_rm1, dch_bytes, 5);
      state->ysf_rm1[5] = '\0';

      memcpy (state->ysf_rm2, dch_bytes+5, 5);
      state->ysf_rm2[5] = '\0';

      break;
    case 5:
      //Remarks 3 and 4
      memcpy (rem1, dch_bytes, 5);
      rem1[5] = '\0';
      fprintf (stderr, "RM3: ");
      fprintf (stderr, "%s ", rem1);
      
      memcpy (rem2, dch_bytes+5, 5);
      rem2[5] = '\0';
      fprintf (stderr, "RM4: ");
      fprintf (stderr, "%s ", rem2);

      memcpy (state->ysf_rm3, dch_bytes, 5);
      state->ysf_rm3[5] = '\0';

      memcpy (state->ysf_rm4, dch_bytes+5, 5);
      state->ysf_rm4[5] = '\0';
      break;

    default:
      break;
  }

}

static inline uint16_t crc16ysf(const uint8_t buf[], int len)
{
  uint32_t poly = (1<<12) + (1<<5) + (1<<0);
  uint32_t crc = 0;
  for(int i=0; i<len; i++) 
  {
    uint8_t bit = buf[i] & 1;
    crc = ((crc << 1) | bit) & 0x1ffff;
    if (crc & 0x10000) crc = (crc & 0xffff) ^ poly;
  }
  crc = crc ^ 0xffff;
  return crc & 0xffff;
}

//modified version of nxdn_deperm_facch1 -- this one for V/D Type 2 CC DCH (100 dibit version)
int ysf_conv_dch2 (dsd_opts * opts, dsd_state * state, uint8_t bn, uint8_t bt, uint8_t fn, uint8_t ft, uint8_t cm, uint8_t input[])
{

  int i, j, k, err;
  uint8_t s0, s1;
  uint8_t trellis_buf[100];
  uint8_t temp[210];
  uint8_t m_data[100];
  uint8_t bits[210];
  memset (trellis_buf, 0, sizeof(trellis_buf));
  memset (temp, 0, sizeof (temp));
  memset (m_data, 0, sizeof (m_data));
  memset (bits, 0, sizeof(bits));
  err = 0;

  //dibit de-interleave block length M = 9 dibits and depth N = 20
  uint8_t buf[100];
  memset (buf, 0, sizeof(buf));
  for (i=0; i<20; i++) {
    for (j=0; j<5; j++) {
      buf[j+(i*5)] = input[i+(j*20)];
    }
  }

  k = 0;
  //convert dibits to bits
  for (i = 0; i < 100; i++)
  {
    bits[k++] = (buf[i] >> 1) & 1;
    bits[k++] = (buf[i] >> 0) & 1;
  }

  //setup for the convolutional decoder
  for (i = 0; i < 200; i++)
    temp[i] = bits[i] << 1;

  CNXDNConvolution_start();
  for (i = 0; i < 100; i++)
  {
    s0 = temp[(2*i)+0];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }

  CNXDNConvolution_chainback(m_data, 96);

  //96/8 = 12, last 4 (96-100) are trailing zeroes
  for(i = 0; i < 12; i++) 
  {
    trellis_buf[(i*8)+0] = (m_data[i] >> 7) & 1;
    trellis_buf[(i*8)+1] = (m_data[i] >> 6) & 1;
    trellis_buf[(i*8)+2] = (m_data[i] >> 5) & 1;
    trellis_buf[(i*8)+3] = (m_data[i] >> 4) & 1;
    trellis_buf[(i*8)+4] = (m_data[i] >> 3) & 1;
    trellis_buf[(i*8)+5] = (m_data[i] >> 2) & 1;
    trellis_buf[(i*8)+6] = (m_data[i] >> 1) & 1;
    trellis_buf[(i*8)+7] = (m_data[i] >> 0) & 1;
  }

  uint16_t crc = crc16ysf(trellis_buf, 96);
  if (crc != 0) err = -2;	// crc failure

  for (i = 0; i < 80; i++)
    trellis_buf[i] = trellis_buf[i] ^ pn95[i];

  //reload after de-whitening
  memset (m_data, 0, sizeof (m_data));
  for (i = 0; i < 12; i++)
    m_data[i] = (uint8_t)ConvertBitIntoBytes(&trellis_buf[i*8], 8);

  //decode the callsign, etc, found in the DCH when no errors
  if (err == 0)
    ysf_dch_decode2 (state, bn, bt, fn, ft, cm, trellis_buf);
  else
  {
    fprintf (stderr, "%s", KRED);
    fprintf (stderr, "DCH (CRC ERR) ");
    fprintf (stderr, "%s", KNRM);
  }

  if (opts->payload == 1)
  {
    fprintf (stderr, "\n ");
    fprintf (stderr, "DCH2: ");
    for (i = 0; i < 12; i++)
      fprintf (stderr, "[%02X]", m_data[i]); 

  }

	return err;
}

//modified version of nxdn_deperm_facch1 -- this one for Full Rate, Type 1 CC, Headers and Terminators DCH (180 dibit version)
int ysf_conv_dch (dsd_opts * opts, dsd_state * state, uint8_t bn, uint8_t bt, uint8_t fn, uint8_t ft, uint8_t cm, uint8_t input[])
{
  int i, j, k, err;
  uint8_t s0, s1;
  uint8_t trellis_buf[190];
  uint8_t temp[370];
  uint8_t m_data[100];
  uint8_t bits[370];
  memset (trellis_buf, 0, sizeof(trellis_buf));
  memset (temp, 0, sizeof (temp));
  memset (m_data, 0, sizeof (m_data));
  memset (bits, 0, sizeof(bits));
  err = 0;

  //dibit de-interleave block length M = 9 dibits and depth N = 20
  uint8_t buf[180];
  memset (buf, 0, sizeof(buf));
  for (i=0; i<20; i++) { //20*9 = 180
    for (j=0; j<9; j++) { 
      buf[j+(i*9)] = input[i+(j*20)];
    }
  }

  k = 0;
  //convert dibits to bits
  for (i = 0; i < 180; i++)
  {
    bits[k++] = (buf[i] >> 1) & 1;
    bits[k++] = (buf[i] >> 0) & 1;
  }

  //setup for the convolutional decoder
  for (i = 0; i < 360; i++)
    temp[i] = bits[i] << 1;

  CNXDNConvolution_start();
  for (i = 0; i < 180; i++)
  {
    s0 = temp[(2*i)+0];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }

  CNXDNConvolution_chainback(m_data, 176);

  //176/8 = 22, last 4 (176-180) are trailing zeroes
  for(i = 0; i < 22; i++) 
  {
    trellis_buf[(i*8)+0] = (m_data[i] >> 7) & 1;
    trellis_buf[(i*8)+1] = (m_data[i] >> 6) & 1;
    trellis_buf[(i*8)+2] = (m_data[i] >> 5) & 1;
    trellis_buf[(i*8)+3] = (m_data[i] >> 4) & 1;
    trellis_buf[(i*8)+4] = (m_data[i] >> 3) & 1;
    trellis_buf[(i*8)+5] = (m_data[i] >> 2) & 1;
    trellis_buf[(i*8)+6] = (m_data[i] >> 1) & 1;
    trellis_buf[(i*8)+7] = (m_data[i] >> 0) & 1;
  }

  uint16_t crc = crc16ysf(trellis_buf, 176);
  if (crc != 0) err = -2;	// crc failure

  for (i = 0; i < 160; i++)
    trellis_buf[i] = trellis_buf[i] ^ pn95[i];

  //reload after de-whitening
  memset (m_data, 0, sizeof (m_data));
  for (i = 0; i < 22; i++)
    m_data[i] = (uint8_t)ConvertBitIntoBytes(&trellis_buf[i*8], 8);

  //decode the callsign, etc, found in the DCH when no errors
  if (err == 0)
    ysf_dch_decode (state, bn, bt, fn, ft, cm, trellis_buf);
  else
  {
    fprintf (stderr, "%s", KRED);
    fprintf (stderr, "DCH (CRC ERR) ");
    fprintf (stderr, "%s", KNRM);
  }

  if (opts->payload == 1)
  {
    fprintf (stderr, "\n ");
    fprintf (stderr, "DCH1: ");
    for (i = 0; i < 22; i++)
      fprintf (stderr, "[%02X]", m_data[i]); 

  }

	return err;
}

//modified version of nxdn_deperm_facch1
int ysf_conv_fich (uint8_t input[], uint8_t dest[32])
{
  int i, j, k, err;
  uint8_t s0, s1;
  uint8_t trellis_buf[100];
  uint8_t temp[210];
  uint8_t m_data[100];
  uint8_t bits[210];
  memset (trellis_buf, 0, sizeof(trellis_buf));
  memset (temp, 0, sizeof (temp));
  memset (m_data, 0, sizeof (m_data));
  memset (bits, 0, sizeof(bits));
  err = 0;

  //dibit de-interleave block length M = 5 dibits and depth N = 10
  uint8_t buf[100];
  memset (buf, 0, sizeof(buf));
  for (i=0; i<20; i++) {
    for (j=0; j<5; j++) {
      buf[j+(i*5)] = input[i+(j*20)];
    }
  }

  k = 0;
  //convert dibits to bits
  for (i = 0; i < 100; i++)
  {
    bits[k++] = (buf[i] >> 1) & 1;
    bits[k++] = (buf[i] >> 0) & 1;
  }

  //setup for the convolutional decoder
  for (i = 0; i < 200; i++) //192
    temp[i] = bits[i] << 1;

  CNXDNConvolution_start();
  for (i = 0; i < 100; i++)
  {
    s0 = temp[(2*i)+0];
    s1 = temp[(2*i)+1];

    CNXDNConvolution_decode(s0, s1);
  }

  CNXDNConvolution_chainback(m_data, 96);

  //96/8 = 12, last 4 (96-100) are trailing zeroes
  for(i = 0; i < 12; i++) 
  {
    trellis_buf[(i*8)+0] = (m_data[i] >> 7) & 1;
    trellis_buf[(i*8)+1] = (m_data[i] >> 6) & 1;
    trellis_buf[(i*8)+2] = (m_data[i] >> 5) & 1;
    trellis_buf[(i*8)+3] = (m_data[i] >> 4) & 1;
    trellis_buf[(i*8)+4] = (m_data[i] >> 3) & 1;
    trellis_buf[(i*8)+5] = (m_data[i] >> 2) & 1;
    trellis_buf[(i*8)+6] = (m_data[i] >> 1) & 1;
    trellis_buf[(i*8)+7] = (m_data[i] >> 0) & 1;
  }

  uint8_t fich_bits[12*4];
  uint8_t temp_b[24];
  bool g[4];

  // run golay 24_12 error correction
  for (i = 0; i < 4; i++)
  {
    memset (temp, 0, sizeof(temp_b));
    g[i] = FALSE;

    for (j = 0; j < 24; j++)
      temp_b[j] = (char) trellis_buf[(i*24)+j];

    g[i] = Golay_24_12_decode(temp_b);
    if(g[i] == FALSE) err = -1;

    for (j = 0; j < 24; j++)
      trellis_buf[(i*24)+j] = (uint8_t) temp_b[j];
  }

  //load corrected bits
  for (i = 0; i < 12; i++)
  {
    fich_bits[(12*0)+i] = trellis_buf[i+0];
    fich_bits[(12*1)+i] = trellis_buf[i+24];
    fich_bits[(12*2)+i] = trellis_buf[i+48];
    fich_bits[(12*3)+i] = trellis_buf[i+72];
  }

  uint16_t crc = crc16ysf(fich_bits, 48);
  if (crc != 0) err = -2;	// crc failure

  memset (m_data, 0, sizeof (m_data));
  for (i = 0; i < 12; i++)
    m_data[i] = (uint8_t)ConvertBitIntoBytes(&trellis_buf[i*8], 8);

	memcpy(dest, fich_bits, 32); //copy minus the crc16
	return err;
}

//YSF pn95 scrambler/whitening bit generator with seed 111001001 
void pn95_lfsr() //test to see if this generates the correct bits now
{
  int i;
  int lfsr;
  int pN[513];
  memset (pN, 0, sizeof(pN));
  int bit = 0;

  lfsr = 0x1c9; //111001001 initial value
  fprintf (stderr, "\n pN95 = { \n");
  for (i = 0; i < 512; i++)
  {
    pN[i] = lfsr & 0x1;
    bit = ( (lfsr >> 4) ^ (lfsr >> 0) ) & 1;
    lfsr =  ( (lfsr >> 1 ) | (bit << 9) ); //9, or 8
    if (i % 8 == 0) fprintf (stderr, "\n");
    fprintf (stderr, " %d,", pN[i]);
  }
  fprintf (stderr, "};");

}


void ysf_ehr (dsd_opts * opts, dsd_state * state, uint8_t dbuf[180], int start, int stop )
{
  int i;
  char ambe_fr[4][24];
  memset (ambe_fr, 0, sizeof(ambe_fr));

  const int *w, *x, *y, *z;

  int st = state->synctype;
  state->synctype = 28;

  uint8_t b1, b2;
  for (; start < stop; start++)
  {
    w = YnW;
    x = YnX;
    y = YnY;
    z = YnZ;

    //debug
    // fprintf (stderr, " DBUF = ");

    for (i = 0; i < 36; i++)
    {

      //debug
      // fprintf (stderr, "%d", dbuf[(start*36)+i]);

      b1 = dbuf[(start*36)+i] >> 1;
      b2 = dbuf[(start*36)+i] & 1;

      //should all be loaded back to back
      ambe_fr[*w][*x] = (char)b1;
      ambe_fr[*y][*z] = (char)b2;
  
      w++;
      x++;
      y++;
      z++;

    }

    processMbeFrame (opts, state, NULL, ambe_fr, NULL);

    if (opts->floating_point == 0)
    {
      // processAudio(opts, state); //needed here? -- nothign to test it with

      if (opts->wav_out_f != NULL)
        writeSynthesizedVoice (opts, state);

      if (opts->pulse_digi_out_channels == 1)
        playSynthesizedVoiceMS(opts, state);

      if(opts->pulse_digi_out_channels == 2)
        playSynthesizedVoiceSS(opts, state);
    }

    if (opts->floating_point == 1) //float audio is really quiet now (look into it)
    {

      memcpy (state->f_l, state->audio_out_temp_buf, sizeof(state->f_l));
      
      if (opts->pulse_digi_out_channels == 1)
        playSynthesizedVoiceFM(opts, state);

      if(opts->pulse_digi_out_channels == 2)
        playSynthesizedVoiceFS(opts, state);
    }

  }
  
  if (opts->payload == 1)
  {
    fprintf(stderr, "\n");
  }
  
  state->synctype = st;
}

void processYSF(dsd_opts * opts, dsd_state * state)
{
  //TODO: Reorganize and remove unused variables and arrays, etc
  static uint8_t last_dt, last_fi; //if we can't get a good dt and fi, then just use the last one instead
  int i, j, k, l, err, vstart, vstop, dstart, dstop; //start stops are for Full Rate when we might have some portions of Data present in the Comm Channel
  int dibit;
  uint8_t fichdibits[100]; //fich dibits
  uint8_t fichrawdibits[100];
  uint8_t fichrawbits[200];
  uint8_t fich_d_bits[100];
  uint8_t fich_decode[32];
  memset (fichdibits, 0, sizeof(fichdibits));
  memset (fichrawdibits, 0, sizeof(fichrawdibits));
  memset (fichrawbits, 0, sizeof(fichrawbits));
  memset (fich_d_bits, 0, sizeof(fich_d_bits));
  memset (fich_decode, 0, sizeof(fich_decode));
  char ambe_fr[4][24];
  memset (ambe_fr, 0, sizeof(ambe_fr));
  char imbe_fr[8][23];
  memset (imbe_fr, 0, sizeof(imbe_fr));
  uint8_t b1, b2, msb, lsb;
  b1 = b2 = msb = lsb = vstart = vstop = dstart = dstop = 0;

  //fich information
  uint8_t fi = 9; //Header, Communications, or Terminator
  // uint8_t cs = 9; //Type of Callsign
  uint8_t cm = 9; //Type of Call
  uint8_t bn = 9; //block number
  uint8_t bt = 9; //block total
  uint8_t fn = 9; //frame number
  uint8_t ft = 9; //frame total
  uint8_t mr = 9; //message path
  uint8_t vp = 9; //VoIP path
  uint8_t dt = 9; //Data Type -- type 1 (EHR no VeCH); type 2 (EHR w/ VeCH); type 3 (EFR)
  uint8_t st = 9; //SQL Type
  uint8_t sc = 69; //SQL Code

  // pn95lfsr(); //run to print the bits for the DCH/VeCH whitening array

  //FICH you
  for (i = 0; i < 100; i++)
    fichrawdibits[i] = getDibit(opts, state);

  //from nxdn_deperm_facch1 w/ nxdn convolutional decoder
  err = ysf_conv_fich (fichrawdibits, fich_decode);

  //if errors decoding fich, then just treat it like the last frame that came in
  if (err == 0)
  {
    fi = (uint8_t)ConvertBitIntoBytes(&fich_decode[0], 2);
    // cs = (uint8_t)ConvertBitIntoBytes(&fich_decode[2], 2);
    cm = (uint8_t)ConvertBitIntoBytes(&fich_decode[4], 2);
    bn = (uint8_t)ConvertBitIntoBytes(&fich_decode[6], 2);
    bt = (uint8_t)ConvertBitIntoBytes(&fich_decode[8], 2);
    fn = (uint8_t)ConvertBitIntoBytes(&fich_decode[10], 3);
    ft = (uint8_t)ConvertBitIntoBytes(&fich_decode[13], 3);
    mr = (uint8_t)ConvertBitIntoBytes(&fich_decode[18], 3);
    vp = fich_decode[21];
    dt = (uint8_t)ConvertBitIntoBytes(&fich_decode[22], 2);
    st = fich_decode[24];
    sc = (uint8_t)ConvertBitIntoBytes(&fich_decode[25], 7);

    state->ysf_dt = dt;
    state->ysf_fi = fi;
    state->ysf_cm = cm;

    last_dt = dt;
    last_fi = fi;
  }
  else
  {
    dt = last_dt;
    fi = last_fi;
  }

  //print some useful decoded stuff
  if (dt == 0 && err == 0) fprintf (stderr, "V/D1 "); //Voice/Data Type 1
  if (dt == 1 && err == 0) fprintf (stderr, "DATA "); //Full Rate Data
  if (dt == 2 && err == 0) fprintf (stderr, "V/D2 "); //Voice/Data Type 2
  if (dt == 3 && err == 0) fprintf (stderr, "VWFR "); //Full Rate Voice

  if (cm == 0) fprintf (stderr, "Group/CQ ");
  if (cm == 3) fprintf (stderr, "Private  ");
  if (cm == 1) fprintf (stderr, "RID Mode "); //Radio ID Mode -- updated in the 1V02 spec manual
  if (cm == 2) fprintf (stderr, "Res: 2   ");

  // if (vp == 0) fprintf (stderr, "Local (Simplex) ");
  // if (vp == 1) fprintf (stderr, "Internet (Rep)  ");

  if (vp == 0) fprintf (stderr, "-Simplex ");
  if (vp == 1) fprintf (stderr, "Repeater ");

  //disabling below lines, seems mostly redundant, 
  //any Simplex is Direct Wave, and if its VoIP, then the Uplink is always busy (I think)

  // if (mr == 0) fprintf (stderr, "(Direct Wave) ");
  // if (mr == 1) fprintf (stderr, "(Uplink Free) ");
  // if (mr == 2) fprintf (stderr, "(Uplink Busy) ");
  if (mr > 2 && mr < 7) fprintf (stderr, "Res: %03d ", mr);

  if (fi == 0 && err == 0) fprintf (stderr, "HC "); //Header
  if (fi == 1 && err == 0) fprintf (stderr, "CC "); //Communication
  if (fi == 2 && err == 0) fprintf (stderr, "TC "); //Terminator
  if (fi == 3 && err == 0) fprintf (stderr, "XX "); //Test

  if (st && sc != 69) fprintf (stderr, "SQL ");
  if (st && sc != 69) fprintf (stderr, "CODE: %03d ", sc);

  //simplified version
  if (err == 0 && opts->payload == 1)
    fprintf (stderr, "FN:%d-%d ", fn, ft);

  if (err != 0)
  {
    fprintf (stderr, "%s", KRED);
    fprintf (stderr, "FICH ");
    if (err == -1)
      fprintf (stderr, "(FEC ERR) ");
    if (err == -2)
      fprintf (stderr, "(CRC ERR) ");
    fprintf (stderr, "%s", KNRM);
  }

  if (opts->payload == 1)
  {
    fprintf (stderr, " FICH: ");
    for (int i = 0; i < 4; i++)
      fprintf (stderr, "[%02X]", (uint8_t)ConvertBitIntoBytes(&fich_decode[i*8], 8)); 
  }

  // if (fi == 0) fprintf (stderr, "%s", KGRN); //HC Channel
  // else if (fi == 2) fprintf (stderr, "%s", KRED); //TC Channel
  // else if (dt == 0 || dt == 2) fprintf (stderr, "%s", KGRN); //Voice CC
  // else fprintf (stderr, "%s", KCYN); //Data

  //DCH and VCH
  uint8_t dbuf[190]; //data dibit buffer
  uint8_t vbuf[190]; //voice dibit buffer
  uint8_t vech[255]; //VeCH dibit buffer
  uint8_t temp[512]; ///temp space for manipulating VeCH into VBUF

  char ambe_d[49];

  memset (vbuf, 0, sizeof(vbuf));
  memset (dbuf, 0, sizeof(dbuf));
  memset (vech, 0, sizeof(vech));
  memset (temp, 0, sizeof(temp));

  //bit buffers
  // uint8_t dch_bits[6][72];
  uint8_t dch_bits[160];
  uint8_t vch_bits[6][72];
  uint8_t vech_bits[104];

  memset (dch_bits, 0, sizeof(dch_bits));
  memset (vch_bits, 0, sizeof(vch_bits));
  memset (vech_bits, 0, sizeof(vech_bits));

  // V/D Mode Type 1 (no VeCH)
  if (fi == 1 && dt == 0)
  {
    //need to double check all this if I can get a sample of it
    for (i = 0; i < 5; i++) //5 on EHR Type 1 (no VeCH)
    {

      // DCH First
      for (j = 0; j < 36; j++)
        dbuf[(i*36)+j] = getDibit(opts, state);


      //VCH Second
      for (j = 0; j < 36; j++)
        vbuf[(i*36)+j] = getDibit(opts, state);

    }

    // send VCH to ehr voice handler
    ysf_ehr (opts, state, vbuf, 0, 4);

    //send DCH to decoder
    ysf_conv_dch (opts, state, bn, bt, fn, ft, cm, dbuf);

  }
  

  /*
    In the case of V/D mode type 2, error correction for the purpose of improving the connectivity
    of a weak electric field is carried out (separately from the error correction function of the voice
    encoder).
  */

  // V/D Mode Type 2 (w/ VeCH and bs ECC)
  if (fi == 1 && dt == 2)
  {
    int d = 0;
    for (i = 0; i < 5; i++)
    {
      //DCH
      for (j = 0; j < 20; j++) 
        dbuf[d++] = getDibit(opts, state);

      //VeCH
      k = 0; l = 0;
      memset (vech_bits, 0, sizeof(vech_bits));
      memset (temp, 0, sizeof(temp));

      state->errs  = 0;
      state->errs2 = 0;

      for (j = 0; j < 52; j++)
      {
        dibit = getDibit(opts, state);

        //deinterleave and de-whiten VeCH (from DSDcc)
        b1 = (dibit >> 1) & 1;
        b2 = (dibit >> 0) & 1;

        msb = vd2Interleave[k]; k++;
        lsb = vd2Interleave[k]; k++;

        vech_bits[msb] = b1 ^ pn95[msb];
        vech_bits[lsb] = b2 ^ pn95[lsb];

      }

      uint8_t majority[8] = {0,0,0,1,0,1,1,1};
      //I have no idea how this is considered to be a form of forward error correction
      l = 0;
      for (j = 0; j < 81; j++)
      {
        //OP25 majority method with table
        if (j % 3 == 2)
        {
          //I still have no idea what the method of this is...hamming table?
          temp[l] = majority[ (vech_bits[j-2] << 2) | (vech_bits[j-1] << 1) | vech_bits[j] ];
          l++;
        }
      }

      for (j = 0; j < 22; j++)
        temp[j+27] = vech_bits[j+81];
      
      for (j = 0; j < 49; j++)
        ambe_d[j] = temp[j]; //(char) 

      state->errs2 = vech_bits[103]; //should be zero, but if it isn't, then its an error

      state->debug_audio_errors += state->errs2;

      mbe_processAmbe2450Dataf (state->audio_out_temp_buf, &state->errs, &state->errs2, state->err_str,
                                ambe_d, state->cur_mp, state->prev_mp, state->prev_mp_enhanced, opts->uvquality);

      if (opts->payload == 1)
        PrintAMBEData (opts, state, ambe_d);

      if (opts->floating_point == 0)
      {
        processAudio(opts, state);

        if (opts->wav_out_f != NULL)
          writeSynthesizedVoice (opts, state);
        if (opts->pulse_digi_out_channels == 1)
          playSynthesizedVoiceMS(opts, state);

        if(opts->pulse_digi_out_channels == 2)
          playSynthesizedVoiceSS(opts, state);
      }
      
      if (opts->floating_point == 1)
      {
        memcpy (state->f_l, state->audio_out_temp_buf, sizeof(state->f_l));

        if (opts->pulse_digi_out_channels == 1)
          playSynthesizedVoiceFM(opts, state);

        if(opts->pulse_digi_out_channels == 2)
          playSynthesizedVoiceFS(opts, state);
        }

    }

    //process completed DCH
    ysf_conv_dch2 (opts, state, bn, bt, fn, ft, cm, dbuf);

  }
  
  //Full-Rate AMBE+2 EFR (Works with IMBE decoder)
  uint8_t imbe_vch[144];
  memset (imbe_vch, 0, sizeof(imbe_vch));
  uint8_t imbe_raw[144];


  //Voice FR Mode (Type 3 AMBE+2 Full Rate)
  if (fi == 1 && dt == 3)
  {

    /*
      Under this pattern, when the initial HC and initial CC (Sub header (CSD3)) cannot be received, the Call sign information can not be obtained.
      No CSD3 information can be obtained from other frames except the FT=1/FN=0 frame.

      What we need to do is look at the frame, and determine if its a CSD3 by the ft == 1 and fn == 0 and set an appropirate start/stop value here
    */

    if (ft == 1 && fn == 0)
    {
      dstart = 0;
      dstop = 6; //5 DCH and 1 reserved bank
      vstart = 0;
      vstop = 2; //only 2 VCH in the CSD3 Sub Header
    }
    else
    {
      dstart = 0;
      dstop = 0;
      vstart = 0;
      vstop = 5;
    }

    for (; dstart < dstop; dstart++)
    {
      //get dibits for CSD3 Sub Header DCH -- still need samples to test this with
      for (j = 0; j < 36; j++) //dbufFR[2][190]
      {
        if (dstart != 5) //only want the first 5
          dbuf[(dstart*36)+j] = getDibit(opts, state);
        else skipDibit(opts, state, 1); //skip the reserved bank
      } 
    }

    for (; vstart < vstop; vstart++)
    {
      
      //init a bunch of stuff
      memset (imbe_raw, 0, sizeof(imbe_raw));
      memset (imbe_fr, 0, sizeof(imbe_fr));

      for (j = 0; j < 72; j++)
      {
        dibit = getDibit(opts, state);

        b1 = (dibit >> 1) & 1;
        b2 = (dibit >> 0) & 1;

        imbe_raw[(j*2)+0] = b1;
        imbe_raw[(j*2)+1] = b2;

      }

      //de-interleave to vch bits
      for (j = 0; j < 144; j++)
        imbe_vch[j] = imbe_raw[fr_interleave[j]];

      k = 0;
      int n, m;
      //load the bits into an imbe_fr backwards
      for (n = 0; n < 4; n++)
      {
        for (m = 22; m >= 0; m--)
          imbe_fr[n][m] = imbe_vch[k++];
      }
      for (n = 4; n < 7; n++)
      {
        for (m = 14; m >= 0; m--)
          imbe_fr[n][m] = imbe_vch[k++];
      }
      for (m = 7; m >= 0; m--)
        imbe_fr[7][m] = imbe_vch[k++];

      //fake it as P25p1 and sent to processMBEFrame
      st = state->synctype;
      state->synctype = 0; //P25p1
      processMbeFrame(opts, state, imbe_fr, NULL, NULL);
      state->synctype = st;

      if (opts->floating_point == 0)
      {
        // processAudio(opts, state); //needed here? -- seems to be running from within mbelib

        if (opts->wav_out_f != NULL)
          writeSynthesizedVoice (opts, state);

        if (opts->pulse_digi_out_channels == 1)
          playSynthesizedVoiceMS(opts, state);

        if(opts->pulse_digi_out_channels == 2)
          playSynthesizedVoiceSS(opts, state);
      }
      
      if (opts->floating_point == 1) //float audio is really quiet now (look into it)
      {

        memcpy (state->f_l, state->audio_out_temp_buf, sizeof(state->f_l));

        if (opts->pulse_digi_out_channels == 1)
          playSynthesizedVoiceFM(opts, state);

        if(opts->pulse_digi_out_channels == 2)
          playSynthesizedVoiceFS(opts, state);
      }

    }

    if (ft == 1 && fn == 0)
    {
      //process completed DCH -- use 2 for bn to switch CSD 3 info
      ysf_conv_dch (opts, state, 2, bt, fn, ft, cm, dbuf);
    }
  }

  //full rate data
  uint8_t dbufFR[2][180];
  memset (dbufFR, 0, sizeof(dbufFR));
  if (dt == 1 || fi == 0 || fi == 2)
  {
    for (i = 0; i < 10; i++)
    {
      for (j = 0; j < 36; j++)
        dbufFR[i % 2][((i/2)*36)+j] = getDibit(opts, state);
    }

    //clear old txt data
    // if (fi == 0) memset (state->ysf_txt, 0, sizeof(state->ysf_txt));
    fprintf (stderr, "\n ");
    for (i = 0; i < 2; i++)
    {
      //process completed DCH -- use i to for bn to switch CSD1 and CSD2 on HC and TC
      if (fi == 0 || fi == 2)
        ysf_conv_dch (opts, state, i, bt, fn, ft, cm, dbufFR[i]);
      //using bn == 2 for full rate data and fn*2+1 for easier frame storage
      else ysf_conv_dch (opts, state, 2, bt, fn*2+i, ft, cm, dbufFR[i]);

    }
    
  }

  //ending line break
  fprintf (stderr, "%s", KNRM);
  fprintf (stderr, "\n");

} //end processYSF
