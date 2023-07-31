#include "dsd.h"
#include "dstar_const.h"
#include "dstar_header.h"

//simplified DSTAR
void processDSTAR(dsd_opts * opts, dsd_state * state)
{
  uint8_t sd[480];
  memset (sd, 0, sizeof(sd));
  int i, j, dibit;
  char ambe_fr[4][24];
  const int *w, *x;
  memset(ambe_fr, 0, sizeof(ambe_fr));
  w = dW;
  x = dX;

  //20 voice and 19 slow data frames (20th is frame sync)
  for (j = 0; j < 21; j++)
  {

    memset(ambe_fr, 0, sizeof(ambe_fr));
    w = dW;
    x = dX;

    for (i = 0; i < 72; i++)
    {
      dibit = getDibit(opts, state);
      ambe_fr[*w][*x] = dibit & 1;
      w++;
      x++;
    }

    soft_mbe(opts, state, NULL, ambe_fr, NULL);

    if (j != 20)
    {
      for (i = 0; i < 24; i++)
      {
        //slow data
        sd[(j*24)+i] = (uint8_t) getDibit(opts, state);

      }
    }

    if (j == 20)
      processDSTAR_SD(opts, state, sd);

  }

  fprintf (stderr, "\n");

}

void processDSTAR_HD(dsd_opts * opts, dsd_state * state)
{

  int i;
  int radioheaderbuffer[660];

  for (i = 0; i < 660; i++)
    radioheaderbuffer[i] = getDibit(opts, state);

  dstar_header_decode(state, radioheaderbuffer);
  processDSTAR(opts, state);

}

//first 24-bits of the larger scramble table
uint8_t sd_d[48] = 
{0,0,0,0, //0
 1,1,1,0, //E
 1,1,1,1, //F
 0,0,1,0, //2
 1,1,0,0, //C
 1,0,0,1};//9

uint16_t crc16ds(uint8_t *buf, int len)
{
  uint32_t poly = 0x1021; //1021, 0811, 8408, 8810
  uint32_t crc = 0x0; //0, of FFFF
  for(int i=0; i<len; i++) 
  {
    uint8_t bit = buf[i] & 1;
    crc = ((crc << 1) | bit) & 0x1ffff;
    if (crc & 0x10000) crc = (crc & 0xffff) ^ poly;
  }
  crc = crc ^ 0xffff;
  return crc & 0xffff;
}

void processDSTAR_SD(dsd_opts * opts, dsd_state * state, uint8_t * sd)
{

  int i, j, len;
  uint8_t sd_bytes[60]; //raw slow data packed into bytes
  uint8_t hd_bytes[51]; //storage for packed bytes sans the header indicator byte (0x55)
  uint8_t rev[480]; //storage for reversing the bit order
  memset (sd_bytes, 0, sizeof(sd_bytes));
  memset (hd_bytes, 0, sizeof(hd_bytes));

  //apply the descrambler
  for (i = 0; i < 480; i++)
    sd[i] ^= sd_d[i%24];
  //reverse the bit order
  for (i = 0; i < 480; i++)
    rev[i] = sd[479-i];
  for (i = 0; i < 480; i++)
    sd[i] = rev[i];

  //load bytes by least significant byte to most significant byte
  //as seen in "The Format of D-Star Slow Data" by Jonathan Naylor
  for (i = 0; i < 60; i++)
    sd_bytes[59-i] = (uint8_t)ConvertBitIntoBytes(&sd[i*8+0], 8);

  len = sd_bytes[0] & 0xF;
  len += 1;

  //load payload bytes without the header indicator byte
  // for (i = 0, j = 1; i < 50; i++)
  // {
  //   hd_bytes[i] = sd_bytes[j];
  //   if (j == 5)  j++;
  //   if (j == 11) j++;
  //   if (j == 17) j++;
  //   if (j == 23) j++;
  //   if (j == 29) j++;
  //   if (j == 35) j++;
  //   if (j == 41) j++;
  //   if (j == 47) j++;
  //   if (j == 53) j++;
  //   j++;
  // }

  //load payload bytes without the header indicator byte
  for (i = 0, j = 0; i < 50; i++)
  {
    j++;
    hd_bytes[i] = sd_bytes[j];
    if (j == len*1-1)  j++;
    if (j == len*2-1)  j++;
    if (j == len*3-1)  j++;
    if (j == len*4-1)  j++;
    if (j == len*5-1)  j++;
    if (j == len*6-1)  j++;
    if (j == len*7-1)  j++;
    if (j == len*8-1)  j++;
    if (j == len*9-1)  j++;    
  }

  uint8_t hd_bits[660];
  memset (hd_bits, 0, sizeof(hd_bits));

  for(i = 0; i < 50; i++) 
  {
    hd_bits[(i*8)+0] = (hd_bytes[i] >> 7) & 1;
    hd_bits[(i*8)+1] = (hd_bytes[i] >> 6) & 1;
    hd_bits[(i*8)+2] = (hd_bytes[i] >> 5) & 1;
    hd_bits[(i*8)+3] = (hd_bytes[i] >> 4) & 1;
    hd_bits[(i*8)+4] = (hd_bytes[i] >> 3) & 1;
    hd_bits[(i*8)+5] = (hd_bytes[i] >> 2) & 1;
    hd_bits[(i*8)+6] = (hd_bytes[i] >> 1) & 1;
    hd_bits[(i*8)+7] = (hd_bytes[i] >> 0) & 1;
  }

  uint16_t crc_ext = 0;
  uint16_t crc_cmp = 0;

  //todo: fix me
  crc_ext = (uint16_t)ConvertBitIntoBytes(&hd_bits[312], 16);
  crc_cmp = crc16ds(hd_bits, 312+16);

  char str1[9];
	char str2[9];
	char str3[9];
	char str4[13];
  char strf[60];
  char strt[60];
  memset (strf, 0x20, sizeof(strf));
  memset (strt, 0x20, sizeof(strf));

	memcpy (str1, hd_bytes+3,  8);
	memcpy (str2, hd_bytes+11, 8);
	memcpy (str3, hd_bytes+19, 8);
	memcpy (str4, hd_bytes+27, 12);
  memcpy (strf, sd_bytes+3,  56); //copy the entire thing as a string -- full
  memcpy (strt, hd_bytes+3,  46); //copy the entire thing as a string -- truncated

	str1[8]  = '\0';
	str2[8]  = '\0';
	str3[8]  = '\0';
	str4[12] = '\0';
  strf[59] = '\0';
  strt[59] = '\0';

  if (sd_bytes[0] == 0x55) //header format
  {

    fprintf (stderr, " RPT 2: %s", str1);
    fprintf (stderr, " RPT 1: %s", str2);
    fprintf (stderr, " DST: %s", str3);
    fprintf (stderr, " SRC: %s", str4);

    //check flags for info
    if (sd_bytes[1] & 0x80) fprintf (stderr, " DATA");
    if (sd_bytes[1] & 0x40) fprintf (stderr, " REPEATER");
    if (sd_bytes[1] & 0x20) fprintf (stderr, " INTERRUPTED");
    if (sd_bytes[1] & 0x10) fprintf (stderr, " CONTROL SIGNAL");
    if (sd_bytes[1] & 0x08) fprintf (stderr, " URGENT");

    if (crc_cmp == crc_ext) //crc_cmp == crc_ext
    {
      memcpy (state->dstar_rpt2, str1, sizeof(str1));
      memcpy (state->dstar_rpt1, str2, sizeof(str2));
      memcpy (state->dstar_dst, str3, sizeof(str3));
      memcpy (state->dstar_src, str4, sizeof(str4));
    }
    else fprintf (stderr, " (CRC ERR)");

  }

  else //any other format (test, nmea gps, etc)
  {

    fprintf (stderr, " %s", strf);
    if (crc_cmp == crc_ext) //crc_cmp == crc_ext
    {
      if (sd_bytes[0] == 0x40) //fixed-form text (5 bytes each piece)
        memcpy (state->dstar_txt, strf, sizeof(strf));
      else memcpy (state->dstar_gps, strt, sizeof(strt));
    }
    else fprintf (stderr, " (CRC ERR)");
  }


  if (opts->payload == 1)
  {
    fprintf (stderr, "\n SD: ");
    for (i = 0; i < 60; i++)      
      fprintf (stderr, "%02X ", sd_bytes[i]);
    fprintf (stderr, "\n SD: ");
    for (i = 0; i < 50; i++)      
      fprintf (stderr, "%02X ", hd_bytes[i]);

    if (crc_cmp != crc_ext)
      fprintf (stderr, "CRC - EXT: %04X CMP: %04X", crc_ext, crc_cmp);
  }

}
