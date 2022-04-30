/*-------------------------------------------------------------------------------
 * ysf.c
 * Yaesu Fusion Decoder for DSD
 *
 * LWVMOBILE
 * 2022-04 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/
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


#include "dsd.h"

const int fichInterleave[100] = {
        0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95,
        1, 6, 11, 16, 21, 26, 31, 36, 41, 46, 51, 56, 61, 66, 71, 76, 81, 86, 91, 96,
        2, 7, 12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 62, 67, 72, 77, 82, 87, 92, 97,
        3, 8, 13, 18, 23, 28, 33, 38, 43, 48, 53, 58, 63, 68, 73, 78, 83, 88, 93, 98,
        4, 9, 14, 19, 24, 29, 34, 39, 44, 49, 54, 59, 64, 69, 74, 79, 84, 89, 94, 99,
};

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

const int vfrInterleave[144] = { //IMBE 7200x4400 full rate interleave schedule
         0,  24,  48,  72,  96, 120,  25,   1,  73,  49, 121,  97,
         2,  26,  50,  74,  98, 122,  27,   3,  75,  51, 123,  99,
         4,  28,  52,  76, 100, 124,  29,   5,  77,  53, 125, 101,
         6,  30,  54,  78, 102, 126,  31,   7,  79,  55, 127, 103,
         8,  32,  56,  80, 104, 128,  33,   9,  81,  57, 129, 105,
        10,  34,  58,  82, 106, 130,  35,  11,  83,  59, 131, 107,
        12,  36,  60,  84, 108, 132,  37,  13,  85,  61, 133, 109,
        14,  38,  62,  86, 110, 134,  39,  15,  87,  63, 135, 111,
        16,  40,  64,  88, 112, 136,  41,  17,  89,  65, 137, 113,
        18,  42,  66,  90, 114, 138,  43,  19,  91,  67, 139, 115,
        20,  44,  68,  92, 116, 140,  45,  21,  93,  69, 141, 117,
        22,  46,  70,  94, 118, 142,  47,  23,  95,  71, 143, 119
};

//half-rate ambe3600x2450 interleave schedule
const int YSFrW[36] = {
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 1,
  0, 1, 0, 1, 0, 2,
  0, 2, 0, 2, 0, 2,
  0, 2, 0, 2, 0, 2
};

// bit index
const int YSFrX[36] = {
  23, 10, 22, 9, 21, 8,
  20, 7, 19, 6, 18, 5,
  17, 4, 16, 3, 15, 2,
  14, 1, 13, 0, 12, 10,
  11, 9, 10, 8, 9, 7,
  8, 6, 7, 5, 6, 4
};

// bit 0
// frame index
const int YSFrY[36] = {
  0, 2, 0, 2, 0, 2,
  0, 2, 0, 3, 0, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3,
  1, 3, 1, 3, 1, 3
};

// bit index
const int YSFrZ[36] = {
  5, 3, 4, 2, 3, 1,
  2, 0, 1, 13, 0, 12,
  22, 11, 21, 10, 20, 9,
  19, 8, 18, 7, 17, 6,
  16, 5, 15, 4, 14, 3,
  13, 2, 12, 1, 11, 0
};

void processYSF(dsd_opts * opts, dsd_state * state) //YSF
{
  //480 dibits - 20 dibit sync leaves 460 dibits (920 bits) each 'superframe'
  fprintf (stderr, "processYSF Function \n");
  uint32_t i, j, k;
  unsigned char dibit;
  const int *w, *x, *y, *z;
  char ambe_fr[8][4][24];
  unsigned char storage[8][4][24]; //may expand later
  unsigned char discard[460]; //just set up for now to hold random dibit grabs with nothing to do but skip them
  unsigned char vch[4][36]; //correct size array for 4*32 dibit VD2 VCH?
  unsigned char fich[200]; //store as bits after deinterleaving
  unsigned char fichraw[100];
  unsigned char fichdibits[100]; //deinterleaved fich dibits
  unsigned char fichbytes[26];
  unsigned int fi = 0;
  unsigned int cs = 0;
  unsigned int cm = 0;
  unsigned int bn = 0;
  unsigned int bt = 0;
  unsigned int fn = 0;
  unsigned int ft = 0;
  unsigned int mr = 0;
  unsigned int ip = 0;
  unsigned int dt = 0; //DT, 2 bytes (according to manual, but has to be two bits)
  unsigned int sq = 0;
  unsigned int sc = 0;
  int vd1 = 1;
  int vd2 = 0;
  unsigned int msbI = 0;
  unsigned int lsbI = 0;
  unsigned char vd2BitsRaw[104];
  opts->inverted_ysf = 0; //here for testing purposes, move to main later
  for (i = 0; i < 100; i++) //100 dibits for fich
  {
    dibit = getDibit (opts, state);

    if (opts->inverted_ysf == 1) //opts->inverted_ysf == 1
    {
      dibit = (dibit ^ 2);
    }
    fichraw[i] = dibit; //raw storage for debug
    fichdibits[fichInterleave[i]] = dibit; //sort into proper order
    //fich[i*2]   = (1 & (dibit >> 1));   // bit 1
    //fich[i*2+1] = (1 & dibit);          // bit 0
    fich[i*2]   = (fichdibits[fichInterleave[i]] >> 1) & 1;
    //fprintf (stderr, "%b", fich[i*2]);
    fich[i*2+1] =  fichdibits[fichInterleave[i]]       & 1;
    //fprintf (stderr, "%b", fich[i*2+1]);
    if (i == 4)
    {
      //datatype = dibit;
    }
  }
  //fprintf (stderr, "\nFICH BYTES = ");
  for (i = 0; i < 25; i++) //bits to bytes
  {
    fichbytes[i] = 0; //initialize to see what isn't beign set
    fichbytes[i] = ( (fich[i*8]   << 7) | (fich[i*8+1] << 6) | (fich[i*8+2] << 5) | (fich[i*8+3] << 4) |
                     (fich[i*8+4] << 3) | (fich[i*8+5] << 2) | (fich[i*8+6] << 1) | (fich[i*8+7] << 0) );
    //fprintf (stderr, "[%02X]", fichbytes[i]);
  }
  //fprintf (stderr, "\n");
  //datatype = (fich[8] << 1) + fich[9];
  dt = fichbytes[2] & 0x03;
  for (i = 0; i < 2; i++)
  {
    //datatype << 1;
    //datatype = datatype | fichraw[i+8];
    //fprintf (stderr, "%02b", fichdibits[i+4]);
    //fprintf (stderr, "%02b", fichdibits[i+11]);
  }
  //fprintf (stderr, "Datatype = %02b\n", dt);
  //quick dirty VD1 method?
  if (0 == 0) //dt == 0 for VD1
  {
   fprintf (stderr, "processYSF VD1 \n");
   for (j = 0; j < 5; j++) //5 DCH and VCH
   {
    w = YSFrW;
    x = YSFrX;
    y = YSFrY;
    z = YSFrZ;
    k = 0;
    for (i = 0; i < 36; i++)
    {
      dibit = getDibit (opts, state);

      if (opts->inverted_ysf == 1) //opts->inverted_ysf == 1
      {
        dibit = (dibit ^ 2);
      }

      storage[j][*w][*x] = (1 & (dibit >> 1));   // bit 1
      storage[j][*y][*z] = (1 & dibit);          // bit 0

    }



      //AMBE VCH
    for (i = 0; i < 36; i++)
    {
      dibit = getDibit (opts, state);

      if (opts->inverted_ysf == 1) //opts->inverted_ysf == 1
      {
        dibit = (dibit ^ 2);
      }

      ambe_fr[j][*w][*x] = (1 & (dibit >> 1));   // bit 1
      ambe_fr[j][*y][*z] = (1 & dibit);          // bit 0
      w++;
      x++;
      y++;
      z++;
    }
    //processMbeFrame (opts, state, NULL, ambe_fr[j], NULL);
  } //end j < 4 loop
 } //end VD1
 //move processMbeFrame to outside of dibit loop to prevent slip
 for (j = 0; j < 5; j++)
 {
   //processMbeFrame (opts, state, NULL, ambe_fr[j], NULL);
 }
 //VD2 method
 if (0 == 3) //dt == 3
 {
   fprintf (stderr, "processYSF VD2 \n");
   for (j = 0; j < 4; j++)
   {
     //VCH[j] and VeCH[j]
     for (i = 0; i < 20; i++) //32?
     {
       dibit = getDibit (opts, state);

       if (opts->inverted_ysf == 1) //opts->inverted_ysf == 1
       {
         dibit = (dibit ^ 2);
       }
       //collect dibits into array for processing
       vch[j][i] = dibit;
       w = YSFrW;
       x = YSFrX;
       y = YSFrY;
       z = YSFrZ;
       msbI = vd2Interleave[(j+1)*i]; //don't this this is remotely correct, really could use the manual right about now
       lsbI = vd2Interleave[(j+1)*i+1];
       vd2BitsRaw[msbI] = ((vch[j][i] >> 1) & 1);
       vd2BitsRaw[lsbI] = (vch[j][i] & 1);
   } // VCH[j]
   //DCH[j]
   for (i = 0; i < 20; i++) //32?
   {
     dibit = getDibit (opts, state);

     if (opts->inverted_ysf == 1) //opts->inverted_ysf == 1
     {
       dibit = (dibit ^ 2);
     }
     discard[i] = dibit;
   } //end DCH[j]
 } //end for j loop

 } //end VD2

} //end processYSF
