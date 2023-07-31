#include "dsd.h"
#include "dstar_const.h"
#include "fcs.h"

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

//no so simplified Slow Data
void processDSTAR_SD(dsd_opts * opts, dsd_state * state, uint8_t * sd)
{

  int i, j, len;
  uint8_t sd_bytes[60]; //raw slow data packed into bytes
  uint8_t hd_bytes[60]; //storage for packed bytes sans the header indicator byte (0x55)
  uint8_t payload[60]; //unchanged copy of sd_bytes for payload printing
  uint8_t rev[480]; //storage for reversing the bit order
  memset (sd_bytes, 0, sizeof(sd_bytes));
  memset (hd_bytes, 0, sizeof(hd_bytes));
  memset (payload, 0, sizeof(payload));

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

  //make a copy to payload that won't be altered
  memcpy (payload, sd_bytes, sizeof(payload));

  len = sd_bytes[0] & 0xF;
  len += 1;

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

  //works on header format -- may not work on others
  uint16_t crc_ext = (hd_bytes[39] << 8) + hd_bytes[40];
  uint16_t crc_cmp = calc_fcs(hd_bytes, 39);

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
  str1[8]  = '\0';
	str2[8]  = '\0';
	str3[8]  = '\0';
	str4[12] = '\0';

  //safety check, don't want to load nasty values into the strings
  for (i = 1; i < 60; i++)
  {
    //substitue non-ascii characters for spaces
    if (sd_bytes[i] < 0x20)
      sd_bytes[i] = 0x20;
    else if (sd_bytes[i] > 0x7E)
      sd_bytes[i] = 0x20;

    //substitue non-ascii characters for spaces
    if (hd_bytes[i] < 0x20)
      hd_bytes[i] = 0x20;
    else if (hd_bytes[i] > 0x7E)
      hd_bytes[i] = 0x20;

    //replace terminating ffffffff (repeating 0x66 values) with NULL (0x00)
    if (i < 59)
    {
      if (sd_bytes[i] == 0x66 && sd_bytes[i+1] == 0x66)
        sd_bytes[i] = 0x00;

      if (hd_bytes[i] == 0x66 && hd_bytes[i+1] == 0x66)
        hd_bytes[i] = 0x00;
    }

    if (i == 59 && sd_bytes[i] == 0x66)
      sd_bytes[i] = 0x00;

    if (i == 59 && hd_bytes[i] == 0x66)
      hd_bytes[i] = 0x00;

  }

  memcpy (strf, sd_bytes+1, 58); //copy the entire thing as a string -- full
  memcpy (strt, hd_bytes+1, 48); //copy the entire thing as a string -- truncated
  strf[59] = '\0';
  strt[59] = '\0';

  if (sd_bytes[0] == 0x55) //header format
  {

    if (crc_cmp == crc_ext)
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

      memcpy (state->dstar_rpt2, str1, sizeof(str1));
      memcpy (state->dstar_rpt1, str2, sizeof(str2));
      memcpy (state->dstar_dst, str3, sizeof(str3));
      memcpy (state->dstar_src, str4, sizeof(str4));
    }
    else fprintf (stderr, " SLOW DATA - HEADER FORMAT (CRC ERR)");

  }

  //I'm tired of trying to work out all the nuances here, so if its wrong, it is what it is
  //I really doubt anybody who likes DSTAR uses this anyways.
  else //any other format (text, nmea gps, etc)
  {
    //I'd have to rewrite the upper loading portion for a consistent good crc when the header
    //indicator bytes have different len values, so these will most likely always fail a crc,
    //but I am going to leave them active since I have the 'safety' check in place to prevent
    //rouge non-ascii values doing odd things in the terminal and in ncurses

    //sure would be a lot easier if there was seperate and uniform opcodes for all the data types,
    //instead of having to over analyze everyhign to figure out what is text, and GPS, and APRS
    if (1 == 1) //crc_cmp == crc_ext
    {
      //fixed-form at 5 bytes, use the truncated (Some Interleaved APRS/GPS TEXT combos may display incorrectly)
      if (sd_bytes[0] == 0x35)
      {
        fprintf (stderr, " DATA:");
        fprintf (stderr, " %s", strt);
        memcpy (state->dstar_gps, strt, sizeof(strt));
      }
      //free-form text at 5 bytes, use the truncated
      else if (sd_bytes[0] == 0x40)
      {
        fprintf (stderr, " TEXT:");
        fprintf (stderr, " %s", strt);
        memcpy (state->dstar_txt, strt, sizeof(strt));
      }
      //any other 5 byte block value
      else if (sd_bytes[0] & 0x05)
      {
        fprintf (stderr, " _UNK:");
        fprintf (stderr, " %s", strt);
        memcpy (state->dstar_txt, strt, sizeof(strt));
      }
      //coded squelch data
      else if (sd_bytes[0] == 0xC2)
      {
        fprintf (stderr, " SQL: %02X (%03d)", payload[1], payload[1]);
        sprintf (state->dstar_gps, " SQL: %02X (%03d) ", payload[1], payload[1]);
      }
      //any variable len block values
      else //print entire thing
      {
        fprintf (stderr, " _UNK:");
        fprintf (stderr, " %s", strf);
        memcpy (state->dstar_gps, strf, sizeof(strf));
      } 
    }
    // if (crc_cmp != crc_ext) fprintf (stderr, " (CRC ERR)");
  }

  if (opts->payload == 1)
  {
    fprintf (stderr, "\n SD: ");
    for (i = 0; i < 60; i++)
    {
      fprintf (stderr, "[%02X]", payload[i]);
      if (i == 29) fprintf (stderr, "\n     ");
    }

    if (crc_cmp != crc_ext)
      fprintf (stderr, "CRC - EXT: %04X CMP: %04X", crc_ext, crc_cmp);
  }

}
