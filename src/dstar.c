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

    //since we are in a long loop, use this to improve response time in ncurses
    if (opts->use_ncurses_terminal == 1)
      ncursesPrinter(opts, state);

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

  char type[7]; char temp[8]; char tempa[8]; char chungus[80]; UNUSED(chungus);
  memset (chungus, 0, sizeof(chungus));
  memset (temp, 0, sizeof(temp));
  memset (tempa, 0, sizeof(tempa));
  memset (type, 0, sizeof(type));
  memcpy (type, sd_bytes+1, 5);
  type[6] = '\0';

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

  else //any other format (text, aprs, etc)
  {
    //fixed-form at 5 bytes, evaluate
    if (sd_bytes[0] == 0x35)
    {

      //might as well just dump both of these outputs
      if (strcmp(type, "$$CRC") == 0) //APRS
      {
        memset (strt, 0x20, sizeof(strt)); //space fill
        fprintf (stderr, " DATA: ");
        for (i = 1; i < 59; i++)
        {
          if (i%6==0) i++;
          if ( (sd_bytes[i] > 0x19) && (sd_bytes[i] < 0x7F) )
          {
            fprintf (stderr, "%c", sd_bytes[i]);
            // strt[i] = sd_bytes[i];
          }
        }
        // strt[59] = '\0';
        // memcpy (state->dstar_gps, strt, sizeof(strt));
      }

      if (strcmp(type, "$$CRC") == 0) //APRS
      {
        fprintf (stderr, "\n APRS - ");
        sprintf (state->dstar_gps, "APRS - ");
        uint8_t aprs[51];
        int k = 0;
        memset (aprs, 0, sizeof(aprs));
        //reshuffle (yet again) into an easier format to manipulate for aprs
        for (i = 1; i < 59; i++)
        {
          if (i % 6 == 0 ) i++;
          aprs[k] = sd_bytes[i];
          if ( (aprs[k] > 0x19) && (aprs[k] < 0x7F) )
            aprs[i] = 0x20;
          k++;
        }
        //start by looking for the ! exclamation point token
        int start = -1;
        for (i = 30; i < 40; i++)
        {
          if (aprs[i] == 0x21)
          {
            start = i+1;
            break;
          }
        }
        //if not found, then skip (bad decode)
        if (start == -1)
          goto SKIP;

        //debug 
        // fprintf (stderr, "S: %d; ", start); //38, or 39

        //LAT
        memcpy (temp, aprs+start, 2);
        start += 2;
        fprintf (stderr, "Lat: %sd ", temp);
        strcat (state->dstar_gps, "Lat: ");
        strcat (state->dstar_gps, temp);
        strcat (state->dstar_gps, "d ");
        memcpy (temp, aprs+start, 2);
        start += 3;
        fprintf (stderr, "%sm ", temp);
        strcat (state->dstar_gps, temp);
        strcat (state->dstar_gps, "m ");
        memcpy (temp, aprs+start, 2);
        start+=1;
        fprintf (stderr, "%ss ", temp);
        strcat (state->dstar_gps, temp);
        strcat (state->dstar_gps, "s ");
        fprintf (stderr, "%c", aprs[start]);
        if (sd_bytes[start] == 0x4E)
          strcat (state->dstar_gps, "N ");
        else if (sd_bytes[start] == 0x53)
          strcat (state->dstar_gps, "S ");
        fprintf (stderr, "; ");

        //LON
        start += 3;
        memcpy (tempa, aprs+start, 3);
        start += 3;
        fprintf (stderr, "Lon: %sd ", tempa);
        strcat (state->dstar_gps, "Lon: ");
        strcat (state->dstar_gps, tempa);
        strcat (state->dstar_gps, "d ");
        memcpy (temp, aprs+start, 2);
        start += 3; //3
        fprintf (stderr, "%sm ", temp);
        strcat (state->dstar_gps, temp);
        strcat (state->dstar_gps, "m ");
        memcpy (temp, aprs+start, 2);
        start += 2;
        fprintf (stderr, "%ss ", temp);
        strcat (state->dstar_gps, temp);
        strcat (state->dstar_gps, "s ");
        fprintf (stderr, "%c", aprs[start]);
        if (aprs[start] == 0x45)
          strcat (state->dstar_gps, "E ");
        else if (aprs[start] == 0x57)
          strcat (state->dstar_gps, "W ");
        fprintf (stderr, "; ");

        //Got the feeling nobody will actually ever see this message, but who knows
        // fprintf(stderr, "\n DEV NOTE: If you see this message, but incorrect lat/lon,\n please submit samples to https://github.com/lwvmobile/dsd-fme/issues/164 ");

        SKIP: ; //do nothing

        //this seems to work okay with a few samples, but wouldn't be surprised if it broke down
        //randomly on different users, depending on location and what else in in $$CRC
      }
      else
      {
        // fprintf (stderr, " %s: ", type);
        memset (strt, 0x20, sizeof(strt)); //space fill
        fprintf (stderr, " TEXT: ");
        for (i = 1; i < 59; i++)
        {
          if (i%6==0) i++;
          if ( (sd_bytes[i] > 0x19) && (sd_bytes[i] < 0x7F) )
          {
            fprintf (stderr, "%c", sd_bytes[i]);
            strt[i] = sd_bytes[i];
          }
        }
        strt[59] = '\0';
        memcpy (state->dstar_txt, strt, sizeof(strt));
      }
    }

    //free-form text at 5 bytes, truncate every 6th position
    else if (sd_bytes[0] == 0x40)
    {
      memset (strt, 0x20, sizeof(strt)); //space fill
      fprintf (stderr, " TEXT: ");
      for (i = 1; i < 59; i++)
      {
        if (i%6==0) i++;
        if ( (sd_bytes[i] > 0x19) && (sd_bytes[i] < 0x7F) )
        {
          fprintf (stderr, "%c", sd_bytes[i]);
          strt[i] = sd_bytes[i];
        }
      }
      strt[59] = '\0';
      memcpy (state->dstar_txt, strt, sizeof(strt));
    }
    //anything else
    else //print entire thing
    {
      fprintf (stderr, " _UNK:");
      fprintf (stderr, " %s", strf);
      // memcpy (state->dstar_txt, strf, sizeof(strf)); //don't copy the unknown garbo strings
    } 

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
