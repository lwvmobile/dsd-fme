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

char * getTimeL(void) //get pretty hh:mm:ss timestamp
{
  time_t t = time(NULL);

  char * curr;
  char * stamp = asctime(localtime( & t));

  curr = strtok(stamp, " ");
  curr = strtok(NULL, " ");
  curr = strtok(NULL, " ");
  curr = strtok(NULL, " ");

  return curr;
}

//getDate has a bug that affects writing to file using fopen 32-bit Ubuntu OS, need to look into
//increasing array size causes date to not print in Linux builds, but fixes cygwin bug, so reverting to 99 until a better fix is worked out.
char * getDateL(void) {
  char datename[99]; //increased to 200 to fix 32-bit Ubuntu/Cygwin when fopen file printing to lrrp.txt
  char * curr2;
  struct tm * to;
  time_t t;
  t = time(NULL);
  to = localtime( & t);
  strftime(datename, sizeof(datename), "%Y/%m/%d", to);
  curr2 = strtok(datename, " ");
  return curr2;
}

//Trellis Decoding still needs work, so don't be surprised by bad decodes
void Process34Data(dsd_opts * opts, dsd_state * state, unsigned char tdibits[98], uint8_t syncdata[48], uint8_t SlotType[20])
{

  uint32_t i, j, k;
  uint32_t CRCExtracted     = 0;
  uint32_t CRCComputed      = 0;
  uint32_t CRCCorrect       = 0;
  uint32_t IrrecoverableErrors = 0;
  uint8_t  DeInteleavedData[196];
  uint8_t  DmrDataBit[144];
  uint8_t  DmrDataByte[18];
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrame = NULL;
  uint8_t  R[3];
  uint8_t  BPTCReservedBits = 0;
  short int slot = 0;

  slot = (short int)state->currentslot;
  /* Check the current time slot */
  if(state->currentslot == 0)
  {
    TSVoiceSupFrame = &state->TS1SuperFrame;
  }
  else
  {
    TSVoiceSupFrame = &state->TS2SuperFrame;
  }

  unsigned char tdibits_reverse[98];
  unsigned char tdibits_inverted[98];
  unsigned char tdibits_to_bits[196];

  for (i = 0; i < 98; i++)
  {
    tdibits_reverse[97-i] = tdibits[i];
  }

  unsigned char TrellisReturn[18];
  CDMRTrellisDecode(tdibits_reverse, TrellisReturn); //NEEDS REVERSE DIBITS!

  for(i = 0, j = 0; i < 18; i++, j+=8)
  {
    DmrDataBit[j + 0] = (TrellisReturn[i] >> 7) & 0x01;
    DmrDataBit[j + 1] = (TrellisReturn[i] >> 6) & 0x01;
    DmrDataBit[j + 2] = (TrellisReturn[i] >> 5) & 0x01;
    DmrDataBit[j + 3] = (TrellisReturn[i] >> 4) & 0x01;
    DmrDataBit[j + 4] = (TrellisReturn[i] >> 3) & 0x01;
    DmrDataBit[j + 5] = (TrellisReturn[i] >> 2) & 0x01;
    DmrDataBit[j + 6] = (TrellisReturn[i] >> 1) & 0x01;
    DmrDataBit[j + 7] = (TrellisReturn[i] >> 0) & 0x01;
  }

  //define our block and padding values
  uint8_t blocks  = state->data_header_blocks[slot] - 1; //subtract 1 for the relevant value in the calc below
  uint8_t padding = state->data_header_padding[slot];

  //shift data in the superframe up a block
  k = 0;
  for(i = 0; i < 16; i++) //16, or 18? seems to have two extra bytes in front currently
  {
    state->dmr_34_rate_sf[slot][i] = state->dmr_34_rate_sf[slot][i+16];
    state->dmr_34_rate_sf[slot][i+16] = state->dmr_34_rate_sf[slot][i+32];
    state->dmr_34_rate_sf[slot][i+32] = state->dmr_34_rate_sf[slot][i+48];
    state->dmr_34_rate_sf[slot][i+48] = state->dmr_34_rate_sf[slot][i+64];
    state->dmr_34_rate_sf[slot][i+64] = state->dmr_34_rate_sf[slot][i+80];

    DmrDataByte[i] = 0;
    for(j = 0; j < 8; j++)
    {
      DmrDataByte[i] = DmrDataByte[i] << 1;
      DmrDataByte[i] = DmrDataByte[i] | (DmrDataBit[k] & 0x01);
      k++;
    }

    state->dmr_34_rate_sf[slot][i+(blocks*16)] = TrellisReturn[i+2]; //plus two to skip the first two bytes
  }

  //Start Polling the 3/4 Rate Super Frame for Data when byte 0 contains values
  //LRRP
  int message_legnth = 0;
  if ( (state->dmr_34_rate_sf[slot][0] & 0x7F) == 0x45) //Start LRRP now
  {

    //find user home directory and append directory and filename.
    FILE * pFile; //put this outside of the if statement?
    if (opts->lrrp_file_output == 1)
    {
      char * filename = "/lrrp.txt";
      char * home_dir = getenv("HOME");
      char * filepath = malloc(strlen(home_dir) + strlen(filename) + 1);
      strncpy (filepath, home_dir, strlen(home_dir) + 1);
      strncat (filepath, filename, strlen(filename) + 1);
      //end find user home directory and append directory and filename

      pFile = fopen (filepath, "a");

      //hard code filename override for those wanting to use other software
      //pFile = fopen("DSDPlus.LRRP", "a");

      //seperate these two values since they cause issues with garbage writing to files in some environments (cygwin)
      fprintf (pFile, "%s\t", getDateL() ); //current date, may find a way to only add this IF no included timestamp in LRRP data?
      fprintf (pFile, "%s\t", getTimeL()) ; //current timestamp, may find a way to only add this IF no included timestamp in LRRP data?
      fprintf (pFile, "%08lld\t", state->dmr_lrrp_source[state->currentslot]); //source address from data header
    }


    fprintf (stderr, "%s ", KMAG);

    //go to number of octets minus padding and crc, confirmed data may need a second rule to skip Serial Numbers and Block CRCs
    for (short i = 12; i < ( ((blocks+1)*16) - (padding+4) ); i++)
    {

      if ( state->dmr_34_rate_sf[slot][i] == 0x0C) //Source and Destination info
      {
        fprintf (stderr, "\n         Source: ");
        fprintf (stderr, " %03d.%03d.%03d.%03d", (state->dmr_34_rate_sf[slot][i+0] & 0x3F), state->dmr_34_rate_sf[slot][i+1], state->dmr_34_rate_sf[slot][i+2], state->dmr_34_rate_sf[slot][i+3]); //strip first two bits off 1st byte
        fprintf (stderr, " [%08d]", (state->dmr_34_rate_sf[slot][i+1] <<16 ) + (state->dmr_34_rate_sf[slot][i+2] << 8) + state->dmr_34_rate_sf[slot][i+3] );
        fprintf (stderr, " - Port %05d ", (state->dmr_34_rate_sf[slot][i+8] << 8) + state->dmr_34_rate_sf[slot][i+9]);
        fprintf (stderr, "\n    Destination: ");
        fprintf (stderr, " %03d.%03d.%03d.%03d", (state->dmr_34_rate_sf[slot][i+4] & 0x3F), state->dmr_34_rate_sf[slot][i+5], state->dmr_34_rate_sf[slot][6], state->dmr_34_rate_sf[slot][i+7]); //strip first two bits off 4th byte??
        fprintf (stderr, " [%08d]", (state->dmr_34_rate_sf[slot][i+5] <<16 ) + (state->dmr_34_rate_sf[slot][i+6] << 8) + state->dmr_34_rate_sf[slot][i+7] );
        fprintf (stderr, " - Port %05d", (state->dmr_34_rate_sf[slot][i+10] << 8) + state->dmr_34_rate_sf[slot][i+11]);

        i = i + 12; //skip 12 bytes here so we don't accidentally trip another flag with data in these bytes
      }

      if (state->dmr_34_rate_sf[slot][i] == 0x0D) //This should lock on to the second 0x0D value hopefully and tell us the message legnth
      {
        message_legnth = state->dmr_34_rate_sf[slot][i+1];
        fprintf (stderr, "\n Message Legnth: %02d", message_legnth);
        i = i + 2; //skip 2 bytes here so we don't accidentally trip another flag with data in these bytes
      }

      if ( state->dmr_34_rate_sf[slot][i] == 0x34 && message_legnth > 21 ) //timestamp, seems to need legnth of 27, shortened to 21 due to other samples
      {
        fprintf (stderr, "\n  LRRP - Timestamp: ");
        fprintf (stderr, "%4d.", (state->dmr_34_rate_sf[slot][i+1] << 6) + (state->dmr_34_rate_sf[slot][i+2] >> 2) ); //4 digit year
        fprintf (stderr, "%02d.", ( ((state->dmr_34_rate_sf[slot][i+2] & 0x3) << 2) + ((state->dmr_34_rate_sf[slot][i+3] & 0xC0) >> 6)) ); //2 digit month
        fprintf (stderr, "%02d",  ( (state->dmr_34_rate_sf[slot][i+3] & 0x30) >> 1) + ((state->dmr_34_rate_sf[slot][i+3] & 0x0E) >> 1)  ); //2 digit day
        fprintf (stderr, " %02d:",( (state->dmr_34_rate_sf[slot][i+3] & 0x01) << 4) + ((state->dmr_34_rate_sf[slot][i+4] & 0xF0) >> 4)  ); //2 digit hour
        fprintf (stderr,  "%02d:",( (state->dmr_34_rate_sf[slot][i+4] & 0x0F) << 2) + ((state->dmr_34_rate_sf[slot][i+5] & 0xC0) >> 6)  ); //2 digit minute
        fprintf (stderr,  "%02d", ( (state->dmr_34_rate_sf[slot][i+5] & 0x3F) << 0) );                                                     //2 digit second

        i = i + 6; //skip 6 bytes here so we don't accidentally trip another flag with data in these bytes
      }

      //is 66 a hemisphere or meridian difference from 0x51? Or different system type?
      if ( (state->dmr_34_rate_sf[slot][i] == 0x51 && message_legnth > 13) ||
           (state->dmr_34_rate_sf[slot][i] == 0x66 && message_legnth > 13)    ) //lattitude and longitude, message_lenght > 13?
      {
        fprintf (stderr, "\n  LRRP -");
        fprintf (stderr, " Lat: ");
        if (state->dmr_34_rate_sf[slot][i+1] & 0x80) //first bit indicates a sign, or hemisphere?
        {
          fprintf (stderr, "-");
          if (opts->lrrp_file_output == 1)
          {
            fprintf (pFile, "-");
          }
        }
        long int lrrplat;
        long int lrrplon;
        double lat_unit = (double)180/(double)4294967295;
        double lon_unit = (double)360/(double)4294967295;
        lrrplat = ( ( ((state->dmr_34_rate_sf[slot][i+1] & 0x7F ) <<  24 ) + (state->dmr_34_rate_sf[slot][i+2] << 16) + (state->dmr_34_rate_sf[slot][i+3] << 8) + state->dmr_34_rate_sf[slot][i+4]) * 1 );
        lrrplon = ( ( (state->dmr_34_rate_sf[slot][i+5]           <<  24 ) + (state->dmr_34_rate_sf[slot][i+6] << 16) + (state->dmr_34_rate_sf[slot][i+7] << 8) + state->dmr_34_rate_sf[slot][i+8]) * 1 );
        fprintf (stderr, "%.5lf ", ((double)lrrplat) * lat_unit);
        if (opts->lrrp_file_output == 1)
        {
          fprintf (pFile, "%.5lf\t", ((double)lrrplat) * lat_unit);
        }
        fprintf (stderr, " Lon: ");
        if (state->dmr_34_rate_sf[slot][i+5] & 0x80) //first bit indicates a sign, or hemisphere?
        {
          //fprintf (stderr, "-");
        }
        fprintf (stderr, "%.5lf", (lrrplon * lon_unit) );
        if (opts->lrrp_file_output == 1)
        {
          fprintf (pFile, "%.5lf\t", (lrrplon * lon_unit) );
        }
        if (state->dmr_34_rate_sf[slot][i+1] & 0x80) //first bit indicates a sign, or hemisphere?
        {
          sprintf ( state->dmr_lrrp[state->currentslot][3], "Lat: -%.5lf Lon: %.5lf ", ((double)lrrplat) * lat_unit , (lrrplon * lon_unit) );
        }
        else sprintf ( state->dmr_lrrp[state->currentslot][3], "Lat: %.5lf Lon: %.5lf ", ((double)lrrplat) * lat_unit , (lrrplon * lon_unit) );        //print for easy copy/paste into browser?
        //fprintf (stderr, " (");
        if (state->dmr_34_rate_sf[slot][i+1] & 0x80) //first bit indicates a sign, or hemisphere?
        {
          fprintf (stderr, " (-%.5lf, %.5lf)", ((double)lrrplat) * lat_unit , (lrrplon * lon_unit) );
        }
        else fprintf (stderr, " (%.5lf, %.5lf)", ((double)lrrplat) * lat_unit , (lrrplon * lon_unit) );

        i = i + 9 + 2; //skip 9 bytes here so we don't accidentally trip another flag with data in these bytes
      }


      if ( state->dmr_34_rate_sf[slot][i] == 0x6C && message_legnth > 13)
      {
        //either Plus is wrong, or I'm wrong on higher velocities exceeding 0xFF. double check double values?
        //fprintf (stderr, "\n  LRRP - Vi %02X Vf %02X Velocity Units (hex)", state->dmr_34_rate_sf[i+1], state->dmr_34_rate_sf[i+2]);
        double velocity = ( ((double)( (state->dmr_34_rate_sf[slot][i+1] ) + state->dmr_34_rate_sf[slot][i+2] )) / ( (double)128));
        if (opts->lrrp_file_output == 1)
        {
          fprintf (pFile, "%.3lf\t ", (velocity * 3.6) );
        }
        fprintf (stderr, "\n  LRRP - Velocity: %.4lf m/s %.4lf km/h %.4lf mph", velocity, (3.6 * velocity), (2.2369 * velocity) );
        sprintf ( state->dmr_lrrp[state->currentslot][4], "Vel: %.4lf kph ", (3.6 * velocity));

        i = i + 3; //skip 3 bytes here so we don't accidentally trip another flag with data in these bytes
      }
      if ( state->dmr_34_rate_sf[slot][i] == 0x56 && message_legnth > 13)
      {
        //check for appropriate terminology here - Heading, bearing, course, or track?
        if (opts->lrrp_file_output == 1)
        {
          fprintf (pFile, "%d\t", state->dmr_34_rate_sf[slot][i+1] * 2);
        }
        fprintf (stderr, "\n  LRRP - Track: %d Degrees", state->dmr_34_rate_sf[slot][i+1] * 2);
        sprintf ( state->dmr_lrrp[state->currentslot][5], "Dir: %d Deg ", state->dmr_34_rate_sf[slot][i+1] * 2);

        //i = ((blocks+1)*12); //skip to end of loop here so we don't accidentally trip another flag with data in these bytes
        i = 99;
      }
      //try for a Control ACK at the very end if nothing else pops, flag still unknown
      if ( (state->dmr_34_rate_sf[slot][i] == 0x36 || state->dmr_34_rate_sf[slot][i] == 0x37) && message_legnth > 6 )
      {
        fprintf (stderr, "\n  LRRP - Control ACK ");
      }
    }
    if (opts->lrrp_file_output == 1)
    {
      fprintf (pFile, "\n");
      fclose (pFile);
    }
  }
  fprintf (stderr, "%s ", KNRM);

  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr,"%s", KCYN);
    fprintf (stderr, "\n Full 3/4 Rate Trellis Payload - Slot [%d]\n  ", slot+1);
    for (i = 0; i < 18; i++) //16, or 18?
    {
      fprintf (stderr, "[%02X]", TrellisReturn[i]);
    }
    fprintf (stderr, "\n  Hex to Ascii - ");
    for (i = 0; i < 18; i++)
    {
      if (TrellisReturn[i] <= 0x7E && TrellisReturn[i] >=0x20)
      {
        fprintf (stderr, "%c", TrellisReturn[i]);
      }
      else fprintf (stderr, ".");
    }
    fprintf (stderr,"%s", KNRM); //change back to normal
  }
  //Full Super Frame - Debug Output
  if (opts->payload == 1 && state->data_block_counter[state->currentslot] == state->data_header_blocks[state->currentslot])
  {
    fprintf (stderr, "%s",KGRN);
    fprintf (stderr, "\n Rate 3/4 Superframe - Slot [%d]\n  ",slot+1);
    for (i = 0; i < ((blocks+1)*16); i++) //(16*4), using 16 since we jump the first two bytes
    {
      fprintf (stderr, "[%02X]", state->dmr_34_rate_sf[slot][i]);
      if (i == 15 || i == 31 || i == 47 || i == 63 || i == 79)
      {
        fprintf (stderr, "\n  "); //line break and two spaces after each 16 bytes
      }
    }
    fprintf (stderr, "%s ", KNRM);
  }
  state->data_block_counter[state->currentslot]++; //increment block counter
}

//This is Data Header
void ProcessDataData(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{
  short int slot = 0;
  slot = (short int)state->currentslot;

  //see if we still need this portion if we are going to use the data block info
  //clear out the 3/4 rate data superframe so we don't repeat old info
  for (short int i = 0; i < 288; i++) //expanded to 288 to prevent crashing
  {
    state->dmr_34_rate_sf[slot][i] = 0;
  }
  //clear out the 1/2 rate data superframe so we don't repeat old info
  for (short int i = 0; i < 288; i++) //expanded to 288 to prevent crashing
  {
    state->dmr_12_rate_sf[slot][i] = 0;
  }

  uint32_t i, j, k;
  uint32_t CRCExtracted     = 0;
  uint32_t CRCComputed      = 0;
  uint32_t CRCCorrect       = 0;
  uint32_t IrrecoverableErrors = 0;
  uint8_t  DeInteleavedData[196];
  uint8_t  DmrDataBit[96];
  uint8_t  DmrDataByte[12];
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrame = NULL;
  uint8_t  R[3];
  uint8_t  BPTCReservedBits = 0;


  /* Check the current time slot */
  if(state->currentslot == 0)
  {
    TSVoiceSupFrame = &state->TS1SuperFrame;
  }
  else
  {
    TSVoiceSupFrame = &state->TS2SuperFrame;
  }

  CRCExtracted = 0;
  CRCComputed = 0;
  IrrecoverableErrors = 0;

  /* Deinterleave DMR data */
  BPTCDeInterleaveDMRData(info, DeInteleavedData);

  /* Extract the BPTC 196,96 DMR data */
  IrrecoverableErrors = BPTC_196x96_Extract_Data(DeInteleavedData, DmrDataBit, R);

  /* Fill the reserved bit (R(0)-R(2) of the BPTC(196,96) block) */
  BPTCReservedBits = (R[0] & 0x01) | ((R[1] << 1) & 0x02) | ((R[2] << 2) & 0x08);

  /* Convert the 96 bit of voice LC Header data into 12 bytes */
  k = 0;
  for(i = 0; i < 12; i++)
  {
    DmrDataByte[i] = 0;
    for(j = 0; j < 8; j++)
    {
      DmrDataByte[i] = DmrDataByte[i] << 1;
      DmrDataByte[i] = DmrDataByte[i] | (DmrDataBit[k] & 0x01);
      k++;
    }
  }

  /* Fill the CRC extracted (before Reed-Solomon (12,9) FEC correction) */
  CRCExtracted = 0;
  for(i = 0; i < 16; i++)
  {
    CRCExtracted = CRCExtracted << 1;
    CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 80] & 1);
  }

  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  CRCExtracted = CRCExtracted ^ 0xCCCC;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0xCCCC);

  /* Convert corrected 12 bytes into 96 bits */
  for(i = 0, j = 0; i < 12; i++, j+=8)
  {
    DmrDataBit[j + 0] = (DmrDataByte[i] >> 7) & 0x01;
    DmrDataBit[j + 1] = (DmrDataByte[i] >> 6) & 0x01;
    DmrDataBit[j + 2] = (DmrDataByte[i] >> 5) & 0x01;
    DmrDataBit[j + 3] = (DmrDataByte[i] >> 4) & 0x01;
    DmrDataBit[j + 4] = (DmrDataByte[i] >> 3) & 0x01;
    DmrDataBit[j + 5] = (DmrDataByte[i] >> 2) & 0x01;
    DmrDataBit[j + 6] = (DmrDataByte[i] >> 1) & 0x01;
    DmrDataBit[j + 7] = (DmrDataByte[i] >> 0) & 0x01;
  }

  //test
  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    //fprintf (stderr, "\n(Data CRC Okay)");
  }
  else if((IrrecoverableErrors == 0))
  {
    //fprintf (stderr, "\n(Data FEC Okay)");
  }
  else {}//fprintf (stderr, ("\n(Data CRC Fail, FEC Fail)"));

  //collect relevant data header info, figure out format, and grab appropriate info from it
  if (state->data_p_head[state->currentslot] == 0)
  {
    fprintf (stderr, "%s ", KGRN);
    state->data_block_counter[state->currentslot] = 1; //reset block counter to 1
    state->data_header_format[state->currentslot] = DmrDataByte[0]; //collect the entire byte, we can check group or individual, response requested as well on this
    state->data_header_sap[state->currentslot] = (DmrDataByte[1] & 0xF0) >> 4; //SAP is IP(4) or other

    // state->data_header_padding[state->currentslot] = DmrDataByte[1] & 0xF; //number of padding octets on last block
    // state->data_header_blocks[state->currentslot] = DmrDataByte[8] & 0x7F; //number of blocks to follow, most useful for when to poll the super array for data
    // fprintf (stderr, "\n  Unconfirmed Data Header: Format %02X - SAP %02X - Block Count %02X - Padding Octets %02X",
    //          state->data_header_format[state->currentslot], state->data_header_sap[state->currentslot],
    //          state->data_header_blocks[state->currentslot], state->data_header_padding[state->currentslot] );

    if ( (state->data_header_format[state->currentslot] & 0xF) ==  0x2)
    {
      state->data_header_padding[state->currentslot] = DmrDataByte[1] & 0xF; //number of padding octets on last block
      state->data_header_blocks[state->currentslot] = DmrDataByte[8] & 0x7F; //number of blocks to follow, most useful for when to poll the super array for data
      fprintf (stderr, "\n  Unconfirmed Data Header: DPF %01X - SAP %02X - Block Count %02X - Padding Octets %02X",
               state->data_header_format[state->currentslot] & 0xF, state->data_header_sap[state->currentslot],
               state->data_header_blocks[state->currentslot], state->data_header_padding[state->currentslot] );
    }

    if ( (state->data_header_format[state->currentslot] & 0xF) ==  0x3)
    {
      state->data_header_padding[state->currentslot] = DmrDataByte[1] & 0xF; //number of padding octets on last block
      state->data_header_blocks[state->currentslot] = DmrDataByte[8] & 0x7F; //number of blocks to follow, most useful for when to poll the super array for data
      fprintf (stderr, "\n  Confirmed Data Header: DPF %01X - SAP %02X - Block Count %02X - Padding Octets %02X",
               state->data_header_format[state->currentslot] & 0xF, state->data_header_sap[state->currentslot],
               state->data_header_blocks[state->currentslot], state->data_header_padding[state->currentslot] );
    }

    if ( (state->data_header_format[state->currentslot] & 0xF) ==  0xD) //DD_HEAD
    {
      state->data_header_padding[state->currentslot] = DmrDataByte[7] & 0xF;
      state->data_header_blocks[state->currentslot] = ( (DmrDataByte[0] & 0x30) | ((DmrDataByte[1] & 0xF) >> 0) ); //what a pain MSB | LSB
      fprintf (stderr, "\n  Short Data - Defined: DPF %01X - SAP %02X - Appended Blocks %02X - Padding Bits %02X",
               state->data_header_format[state->currentslot] & 0xF, state->data_header_sap[state->currentslot],
               state->data_header_blocks[state->currentslot], state->data_header_padding[state->currentslot] );
    }

    if ( (state->data_header_format[state->currentslot] & 0xF) ==  0xE) //SP_HEAD or R_HEAD
    {
      state->data_header_padding[state->currentslot] = DmrDataByte[7] & 0xF;
      state->data_header_blocks[state->currentslot] = ( (DmrDataByte[0] & 0x30) | ((DmrDataByte[1] & 0xF) >> 0) ); //what a pain MSB | LSB
      fprintf (stderr, "\n  Short Data - Raw or Precoded: DPF %01X - SAP %02X - Appended Blocks %02X - Padding Bits %02X",
               state->data_header_format[state->currentslot] & 0xF, state->data_header_sap[state->currentslot],
               state->data_header_blocks[state->currentslot], state->data_header_padding[state->currentslot] );
    }


  }

  //sanity check to prevent random crash in 1/2 rate and 3/4 rate data
  //we want to assign extra large block sizes to a max of 7 so we don't overload the 1/2 or 3/4 array
  if (state->data_header_blocks[state->currentslot] > 7)
  {
    state->data_header_blocks[state->currentslot] = 7; //currently 1/2 only supports a max of 7 blocks per slot
  }


  //set source here so we can reference it in our text dump
  state->dmr_lrrp_source[state->currentslot] = (DmrDataByte[5] <<16 ) + (DmrDataByte[6] << 8) + DmrDataByte[7];
  fprintf (stderr, "%s ", KNRM);
  //end collecting data header info

  fprintf (stderr, "%s ", KMAG);
  if (state->data_header_sap[state->currentslot] == 0x4 && state->data_p_head[state->currentslot] == 0) //4 is IP data
  {
    fprintf (stderr, "\n  IP4 Data - Source: ");
    //fprintf (stderr, " %d.%d.%d.%d", (DmrDataByte[0] & 0x3F), DmrDataByte[1], DmrDataByte[2], DmrDataByte[3]); //not sure if these are right or not
    fprintf (stderr, "[%d]", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] << 8) + DmrDataByte[7] );
    fprintf (stderr, " Destination: ");
    //fprintf (stderr, " %d.%d.%d.%d", (DmrDataByte[4] & 0x3F), DmrDataByte[5], DmrDataByte[6], DmrDataByte[7]); //not sure if these are right or not
    fprintf (stderr, "[%d]", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] <<8 ) + DmrDataByte[4] );

    //part below should probably be inside of above if condition
    if (1 == 1) //already setting by current slot, no need for checking first
    {
      sprintf ( state->dmr_lrrp[state->currentslot][1], "SRC [%d] DST [%d] ",
              ( (DmrDataByte[5] <<16 ) + (DmrDataByte[6] << 8) + DmrDataByte[7]),
              ( (DmrDataByte[2] <<16 ) + (DmrDataByte[3] <<8 ) + DmrDataByte[4]) );
    }

  }

  if (state->data_header_sap[state->currentslot] == 0x5 && state->data_p_head[state->currentslot] == 0) //5 is ARP Address Resolution Protocol
  {
    fprintf (stderr, "\n  ARP - Address Resolution Protocol ");
  }

  if ( (state->data_header_format[state->currentslot] & 0xF ) == 0xD && state->data_p_head[state->currentslot] == 0) //5 is ARP Address Resolution Protocol
  {
    fprintf (stderr, "\n  Short Data - Defined - Source: ");
    //fprintf (stderr, " %d.%d.%d.%d", (DmrDataByte[0] & 0x3F), DmrDataByte[1], DmrDataByte[2], DmrDataByte[3]); //not sure if these are right or not
    fprintf (stderr, "[%d]", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] << 8) + DmrDataByte[7] );
    fprintf (stderr, " Destination: ");
    //fprintf (stderr, " %d.%d.%d.%d", (DmrDataByte[4] & 0x3F), DmrDataByte[5], DmrDataByte[6], DmrDataByte[7]); //not sure if these are right or not
    fprintf (stderr, "[%d]", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] <<8 ) + DmrDataByte[4] );
  }

  if ( (state->data_header_format[state->currentslot] & 0xF) == 0xE && state->data_p_head[state->currentslot] == 0) //5 is ARP Address Resolution Protocol
  {
    fprintf (stderr, "\n  Short Data - Raw or Status/Precoded - Source: ");
    //fprintf (stderr, " %d.%d.%d.%d", (DmrDataByte[0] & 0x3F), DmrDataByte[1], DmrDataByte[2], DmrDataByte[3]); //not sure if these are right or not
    fprintf (stderr, "[%d]", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] << 8) + DmrDataByte[7] );
    fprintf (stderr, " Destination: ");
    //fprintf (stderr, " %d.%d.%d.%d", (DmrDataByte[4] & 0x3F), DmrDataByte[5], DmrDataByte[6], DmrDataByte[7]); //not sure if these are right or not
    fprintf (stderr, "[%d]", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] <<8 ) + DmrDataByte[4] );
  }

  if ( (state->data_header_format[state->currentslot] & 0xF ) == 0x1 && state->data_p_head[state->currentslot] == 0) //5 is ARP Address Resolution Protocol
  {
    fprintf (stderr, "\n  Response Packet - Source:  ");
    //fprintf (stderr, " %d.%d.%d.%d", (DmrDataByte[0] & 0x3F), DmrDataByte[1], DmrDataByte[2], DmrDataByte[3]); //not sure if these are right or not
    fprintf (stderr, "[%d]", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] << 8) + DmrDataByte[7] );
    fprintf (stderr, " Destination: ");
    //fprintf (stderr, " %d.%d.%d.%d", (DmrDataByte[4] & 0x3F), DmrDataByte[5], DmrDataByte[6], DmrDataByte[7]); //not sure if these are right or not
    fprintf (stderr, "[%d]", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] <<8 ) + DmrDataByte[4] );
  }

  if ( (state->data_header_format[state->currentslot] & 0xF ) == 0x0 && state->data_p_head[state->currentslot] == 0) //5 is ARP Address Resolution Protocol
  {
    fprintf (stderr, "\n  Unified Data Transport - Source:  ");
    //fprintf (stderr, " %d.%d.%d.%d", (DmrDataByte[0] & 0x3F), DmrDataByte[1], DmrDataByte[2], DmrDataByte[3]); //not sure if these are right or not
    fprintf (stderr, "[%d]", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] << 8) + DmrDataByte[7] );
    fprintf (stderr, " Destination: ");
    //fprintf (stderr, " %d.%d.%d.%d", (DmrDataByte[4] & 0x3F), DmrDataByte[5], DmrDataByte[6], DmrDataByte[7]); //not sure if these are right or not
    fprintf (stderr, "[%d]", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] <<8 ) + DmrDataByte[4] );
  }

  if ( (state->data_header_format[state->currentslot] & 0x40) == 0x40 && state->data_p_head[state->currentslot] == 0) //check response req bit
  {
    fprintf (stderr, "\n  Response Requested - ");
    fprintf (stderr, " Source:");
    fprintf (stderr, " [%d]", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] << 8) + DmrDataByte[4] );
    fprintf (stderr, " Destination:");
    fprintf (stderr, " [%d]", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] <<8 ) + DmrDataByte[7] );
  }

  //if proprietary header was flagged by the last data header, then process data differently
  //maybe we could add the data header bytes found here into the 12 rate and/or 34 rate superframe
  if (state->data_p_head[state->currentslot] == 1)
  {
    fprintf (stderr, "\n  Proprietary Data Header: SAP %01X - Format %01X - MFID %02X",
             (DmrDataByte[0] & 0xF0) >> 4, DmrDataByte[0] & 0xF, DmrDataByte[1]       );

    state->data_p_head[state->currentslot] = 0;
    state->data_header_sap[state->currentslot] = 0; //reset this to zero so we don't trip the condition below
    state->data_block_counter[state->currentslot]++; //increment the counter since this counts against the data blocks
    //shim the databytes into the approprate 12 Rate Super Frame
    uint8_t blocks = state->data_header_blocks[state->currentslot] - 1;
    for(i = 0; i < 12; i++)
    {
      state->dmr_12_rate_sf[slot][i+(blocks*12)] = DmrDataByte[i];
    }
  }

  //only set this flag after everything else is processed
  if (state->data_header_sap[state->currentslot] == 0x9 && state->data_p_head[state->currentslot] == 0) //9 is Proprietary Packet data
  {
    fprintf (stderr, "\n  Proprietary Packet Data Incoming ");
    state->data_p_head[state->currentslot] = 1; //flag that next data header will be a proprietary data header
  }
  fprintf (stderr, "%s ", KNRM);

  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "%s", KCYN);
    fprintf (stderr, "\nFull Data Header Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
    fprintf (stderr, "%s", KNRM);
  }


}

void Process1Data(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{

  uint32_t i, j, k;
  uint32_t CRCExtracted     = 0;
  uint32_t CRCComputed      = 0;
  uint32_t CRCCorrect       = 0;
  uint32_t IrrecoverableErrors = 0;
  uint8_t  DeInteleavedData[196];
  uint8_t  DmrDataBit[96];
  uint8_t  DmrDataByte[25];
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrame = NULL;
  uint8_t  R[3];
  uint8_t  BPTCReservedBits = 0;
  short int slot = 0;
  slot = (short int)state->currentslot;

  //define our block and padding values
  uint8_t blocks  = state->data_header_blocks[slot] - 1; //subtract 1 for the relevant value in the calc below
  uint8_t padding = state->data_header_padding[slot];
  //shift data in the superframe up a block
  k = 0;
  for(i = 0; i < 24; i++)
  {
    //reuse dmr_12_rate_sf since 12, we can use 2 blocks to load 1 block here
    state->dmr_12_rate_sf[slot][i] = state->dmr_12_rate_sf[slot][i+24];    //shift block 2 to block 1
    state->dmr_12_rate_sf[slot][i+24] = state->dmr_12_rate_sf[slot][i+48];    //shift block 3 to block 2
    state->dmr_12_rate_sf[slot][i+48] = state->dmr_12_rate_sf[slot][i+72];    //shift block 3 to block 2
    DmrDataByte[i] = 0;
    for(j = 0; j < 8; j++)
    {
      DmrDataByte[i] = DmrDataByte[i] << 1;
      DmrDataByte[i] = DmrDataByte[i] | (info[k] ); //& 1
      k++;
    }

    //start loading new superframe at appropriate block count determined by the data header
    state->dmr_12_rate_sf[slot][i+(blocks*24)] = DmrDataByte[i];
  }

  //need to rework this for rate 1 data, will be a crc 9 for each confirmed block,
  //and/or CRC 32 on last block (full superframe)
  //NOTE 2: Only used when CRC-9 present in burst.

  /* Fill the CRC extracted (before Reed-Solomon (12,9) FEC correction) */
  // CRCExtracted = 0;
  // //for(i = 0; i < 24; i++)
  // for(i = 0; i < 16; i++)
  // {
  //   CRCExtracted = CRCExtracted << 1;
  //   //CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 72] & 1);
  //   CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 80] & 1);
  // }

  //CRCExtracted = CRCExtracted ^ 0x6969;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  //CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x969696);

  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "%s", KCYN);
    fprintf (stderr, "\nFull Rate 1 Payload \n");
    for (i = 0; i < 24; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
      if (i == 11)
      {
        fprintf (stderr, "\n");
      }
    }
    fprintf (stderr, "\n  Hex to Ascii - ");
    for (i = 0; i < 24; i++)
    {
      if (DmrDataByte[i] <= 0x7E && DmrDataByte[i] >=0x20)
      {
        fprintf (stderr, "%c", DmrDataByte[i]);
      }
      else fprintf (stderr, ".");
    }
    fprintf (stderr, "%s", KNRM);
  }

  //Full Super Frame - Debug Output
  if (opts->payload == 1 && state->data_block_counter[state->currentslot] == state->data_header_blocks[state->currentslot])
  {
    fprintf (stderr, "%s",KGRN);
    fprintf (stderr, "\n Rate 1 Superframe - Slot [%d]\n  ",slot+1);
    for (i = 0; i < ((blocks+1)*24); i++) //only print as many as we have blocks
    {
      fprintf (stderr, "[%02X]", state->dmr_12_rate_sf[slot][i]);
      if (i == 11 || i == 23 || i == 35 || i == 47 || i == 59 || i == 71) //line break and two spaces after each 12 bytes
      {
        fprintf (stderr, "\n  ");
      }
    }
    fprintf (stderr, "%s ", KNRM);
  }
  state->data_block_counter[state->currentslot]++; //increment block counter

}

void ProcessMBChData(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{

  uint32_t i, j, k;
  uint32_t CRCExtracted     = 0;
  uint32_t CRCComputed      = 0;
  uint32_t CRCCorrect       = 0;
  uint32_t IrrecoverableErrors = 0;
  uint8_t  DeInteleavedData[196];
  uint8_t  DmrDataBit[96];
  uint8_t  DmrDataByte[12];
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrame = NULL;
  uint8_t  R[3];
  uint8_t  BPTCReservedBits = 0;

  /* Check the current time slot */
  if(state->currentslot == 0)
  {
    TSVoiceSupFrame = &state->TS1SuperFrame;
  }
  else
  {
    TSVoiceSupFrame = &state->TS2SuperFrame;
  }

  CRCExtracted = 0;
  CRCComputed = 0;
  IrrecoverableErrors = 0;

  /* Deinterleave DMR data */
  BPTCDeInterleaveDMRData(info, DeInteleavedData);

  /* Extract the BPTC 196,96 DMR data */
  IrrecoverableErrors = BPTC_196x96_Extract_Data(DeInteleavedData, DmrDataBit, R);

  /* Fill the reserved bit (R(0)-R(2) of the BPTC(196,96) block) */
  BPTCReservedBits = (R[0] & 0x01) | ((R[1] << 1) & 0x02) | ((R[2] << 2) & 0x08);

  /* Convert the 96 bit of voice LC Header data into 12 bytes */
  k = 0;
  for(i = 0; i < 12; i++)
  {
    DmrDataByte[i] = 0;
    for(j = 0; j < 8; j++)
    {
      DmrDataByte[i] = DmrDataByte[i] << 1;
      DmrDataByte[i] = DmrDataByte[i] | (DmrDataBit[k] & 0x01);
      k++;
    }
  }

  /* Fill the CRC extracted (before Reed-Solomon (12,9) FEC correction) */
  CRCExtracted = 0;
  for(i = 0; i < 16; i++)
  {
    CRCExtracted = CRCExtracted << 1;
    CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 80] & 1);
  }

  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  CRCExtracted = CRCExtracted ^ 0xAAAA;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0xAAAA);

  /* Convert corrected 12 bytes into 96 bits */
  for(i = 0, j = 0; i < 12; i++, j+=8)
  {
    DmrDataBit[j + 0] = (DmrDataByte[i] >> 7) & 0x01;
    DmrDataBit[j + 1] = (DmrDataByte[i] >> 6) & 0x01;
    DmrDataBit[j + 2] = (DmrDataByte[i] >> 5) & 0x01;
    DmrDataBit[j + 3] = (DmrDataByte[i] >> 4) & 0x01;
    DmrDataBit[j + 4] = (DmrDataByte[i] >> 3) & 0x01;
    DmrDataBit[j + 5] = (DmrDataByte[i] >> 2) & 0x01;
    DmrDataBit[j + 6] = (DmrDataByte[i] >> 1) & 0x01;
    DmrDataBit[j + 7] = (DmrDataByte[i] >> 0) & 0x01;
  }

  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "%s", KCYN);
    fprintf (stderr, "\nFull MBC Header Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
    fprintf (stderr, "%s", KNRM);
  }

}

void ProcessMBCData(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{

  uint32_t i, j, k;
  uint32_t CRCExtracted     = 0;
  uint32_t CRCComputed      = 0;
  uint32_t CRCCorrect       = 0;
  uint32_t IrrecoverableErrors = 0;
  uint8_t  DeInteleavedData[196];
  uint8_t  DmrDataBit[96];
  uint8_t  DmrDataByte[12];
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrame = NULL;
  uint8_t  R[3];
  uint8_t  BPTCReservedBits = 0;

  /* Check the current time slot */
  if(state->currentslot == 0)
  {
    TSVoiceSupFrame = &state->TS1SuperFrame;
  }
  else
  {
    TSVoiceSupFrame = &state->TS2SuperFrame;
  }

  CRCExtracted = 0;
  CRCComputed = 0;
  IrrecoverableErrors = 0;

  /* Deinterleave DMR data */
  BPTCDeInterleaveDMRData(info, DeInteleavedData);

  /* Extract the BPTC 196,96 DMR data */
  IrrecoverableErrors = BPTC_196x96_Extract_Data(DeInteleavedData, DmrDataBit, R);

  /* Fill the reserved bit (R(0)-R(2) of the BPTC(196,96) block) */
  BPTCReservedBits = (R[0] & 0x01) | ((R[1] << 1) & 0x02) | ((R[2] << 2) & 0x08);

  /* Convert the 96 bit of voice LC Header data into 12 bytes */
  k = 0;
  for(i = 0; i < 12; i++)
  {
    DmrDataByte[i] = 0;
    for(j = 0; j < 8; j++)
    {
      DmrDataByte[i] = DmrDataByte[i] << 1;
      DmrDataByte[i] = DmrDataByte[i] | (DmrDataBit[k] & 0x01);
      k++;
    }
  }

  //NO CRC or FEC on MBC Cont. Data?
  //...that seems to be according to the manual

  /* Fill the CRC extracted (before Reed-Solomon (12,9) FEC correction) */
  // CRCExtracted = 0;
  // for(i = 0; i < 16; i++)
  // {
  //   CRCExtracted = CRCExtracted << 1;
  //   CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 80] & 1);
  // }

  //No CRC Mask for MBC Continuation Data
  //Note 1. None required for intermediate or last blocks.
  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  //CRCExtracted = CRCExtracted ^ 0x6969;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  // CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x0); //0, or FFFF?

  /* Convert corrected 12 bytes into 96 bits */
  for(i = 0, j = 0; i < 12; i++, j+=8)
  {
    DmrDataBit[j + 0] = (DmrDataByte[i] >> 7) & 0x01;
    DmrDataBit[j + 1] = (DmrDataByte[i] >> 6) & 0x01;
    DmrDataBit[j + 2] = (DmrDataByte[i] >> 5) & 0x01;
    DmrDataBit[j + 3] = (DmrDataByte[i] >> 4) & 0x01;
    DmrDataBit[j + 4] = (DmrDataByte[i] >> 3) & 0x01;
    DmrDataBit[j + 5] = (DmrDataByte[i] >> 2) & 0x01;
    DmrDataBit[j + 6] = (DmrDataByte[i] >> 1) & 0x01;
    DmrDataBit[j + 7] = (DmrDataByte[i] >> 0) & 0x01;
  }

  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "%s", KCYN);
    fprintf (stderr, "\nFull MBC Continuation Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
    fprintf (stderr, "%s", KNRM);
  }

}

//This data shouldn't be active, but come here to dump in case of data burst type decoding failure
void ProcessReservedData(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{

  uint32_t i, j, k;
  uint32_t CRCExtracted     = 0;
  uint32_t CRCComputed      = 0;
  uint32_t CRCCorrect       = 0;
  uint32_t IrrecoverableErrors = 0;
  uint8_t  DeInteleavedData[196];
  uint8_t  DmrDataBit[96];
  uint8_t  DmrDataByte[12];
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrame = NULL;
  uint8_t  R[3];
  uint8_t  BPTCReservedBits = 0;

  /* Check the current time slot */
  if(state->currentslot == 0)
  {
    TSVoiceSupFrame = &state->TS1SuperFrame;
  }
  else
  {
    TSVoiceSupFrame = &state->TS2SuperFrame;
  }

  /* Convert the 196 bit of voice LC Header data into 12 bytes */
  k = 0;
  for(i = 0; i < 24; i++)
  {
    DmrDataByte[i] = 0;
    for(j = 0; j < 8; j++)
    {
      DmrDataByte[i] = DmrDataByte[i] << 1;
      DmrDataByte[i] = DmrDataByte[i] | info[i];
      k++;
    }
  }

  /* Convert corrected 12 bytes into 96 bits */
  for(i = 0, j = 0; i < 12; i++, j+=8)
  {
    DmrDataBit[j + 0] = (DmrDataByte[i] >> 7) & 0x01;
    DmrDataBit[j + 1] = (DmrDataByte[i] >> 6) & 0x01;
    DmrDataBit[j + 2] = (DmrDataByte[i] >> 5) & 0x01;
    DmrDataBit[j + 3] = (DmrDataByte[i] >> 4) & 0x01;
    DmrDataBit[j + 4] = (DmrDataByte[i] >> 3) & 0x01;
    DmrDataBit[j + 5] = (DmrDataByte[i] >> 2) & 0x01;
    DmrDataBit[j + 6] = (DmrDataByte[i] >> 1) & 0x01;
    DmrDataBit[j + 7] = (DmrDataByte[i] >> 0) & 0x01;
  }

  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "%s", KCYN);
    fprintf (stderr, "\nFull Reserved Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
    fprintf (stderr, "%s", KRED);
    fprintf (stderr, "\nINFO! This Payload Dump May be caused by erroneous data burst type decoding! ");
    fprintf (stderr, "%s", KNRM);
  }

}

void ProcessUnifiedData(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{

  uint32_t i, j, k;
  uint32_t CRCExtracted     = 0;
  uint32_t CRCComputed      = 0;
  uint32_t CRCCorrect       = 0;
  uint32_t IrrecoverableErrors = 0;
  uint8_t  DeInteleavedData[196];
  uint8_t  DmrDataBit[96];
  uint8_t  DmrDataByte[12];
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrame = NULL;
  uint8_t  R[3];
  uint8_t  BPTCReservedBits = 0;
  short int slot = 0;
  slot = (short int)state->currentslot;

  /* Check the current time slot */
  if(state->currentslot == 0)
  {
    TSVoiceSupFrame = &state->TS1SuperFrame;
  }
  else
  {
    TSVoiceSupFrame = &state->TS2SuperFrame;
  }

  CRCExtracted = 0;
  CRCComputed = 0;
  IrrecoverableErrors = 0;

  /* Deinterleave DMR data */
  BPTCDeInterleaveDMRData(info, DeInteleavedData);

  /* Extract the BPTC 196,96 DMR data */
  IrrecoverableErrors = BPTC_196x96_Extract_Data(DeInteleavedData, DmrDataBit, R);

  /* Fill the reserved bit (R(0)-R(2) of the BPTC(196,96) block) */
  BPTCReservedBits = (R[0] & 0x01) | ((R[1] << 1) & 0x02) | ((R[2] << 2) & 0x08);


  k = 0;
  for(i = 0; i < 12; i++)
  {
    DmrDataByte[i] = 0;
    for(j = 0; j < 8; j++)
    {
      DmrDataByte[i] = DmrDataByte[i] << 1;
      DmrDataByte[i] = DmrDataByte[i] | (DmrDataBit[k] & 0x01);
      k++;
    }

  }

  /* Fill the CRC extracted (before Reed-Solomon (12,9) FEC correction) */
  CRCExtracted = 0;
  //for(i = 0; i < 24; i++)
  for(i = 0; i < 16; i++)
  {
    CRCExtracted = CRCExtracted << 1;
    CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 80] & 1);
  }

  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  CRCExtracted = CRCExtracted ^ 0x3333;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x3333);

  //test
  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    //fprintf (stderr, "\n(Unified Data CRC Okay)");
  }
  else if((IrrecoverableErrors == 0))
  {
    //fprintf (stderr, "\n(Unified Data FEC Okay)");
  }
  else
  {
    fprintf (stderr, "%s", KRED);
    fprintf (stderr, ("\n(Unified Rate CRC Fail, FEC Fail)"));
    fprintf (stderr, "%s", KNRM);
  }

  /* Convert corrected 12 bytes into 96 bits */
  for(i = 0, j = 0; i < 12; i++, j+=8)
  {
    DmrDataBit[j + 0] = (DmrDataByte[i] >> 7) & 0x01;
    DmrDataBit[j + 1] = (DmrDataByte[i] >> 6) & 0x01;
    DmrDataBit[j + 2] = (DmrDataByte[i] >> 5) & 0x01;
    DmrDataBit[j + 3] = (DmrDataByte[i] >> 4) & 0x01;
    DmrDataBit[j + 4] = (DmrDataByte[i] >> 3) & 0x01;
    DmrDataBit[j + 5] = (DmrDataByte[i] >> 2) & 0x01;
    DmrDataBit[j + 6] = (DmrDataByte[i] >> 1) & 0x01;
    DmrDataBit[j + 7] = (DmrDataByte[i] >> 0) & 0x01;
  }

  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr,"%s", KCYN);
    fprintf (stderr, "\n Full Unified Data Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
    fprintf (stderr, "\n  Hex to Ascii - ");
    for (i = 0; i < 12; i++)
    {
      if (DmrDataByte[i] <= 0x7E && DmrDataByte[i] >=0x20)
      {
        fprintf (stderr, "%c", DmrDataByte[i]);
      }
      else fprintf (stderr, ".");
    }
    fprintf (stderr,"%s", KNRM);
  }

}

void Process12Data(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{

  uint32_t i, j, k;
  uint32_t CRCExtracted     = 0;
  uint32_t CRCComputed      = 0;
  uint32_t CRCCorrect       = 0;
  uint32_t IrrecoverableErrors = 0;
  uint8_t  DeInteleavedData[196];
  uint8_t  DmrDataBit[96];
  uint8_t  DmrDataByte[12];
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrame = NULL;
  uint8_t  R[3];
  uint8_t  BPTCReservedBits = 0;
  short int slot = 0;
  slot = (short int)state->currentslot;

  /* Check the current time slot */
  if(state->currentslot == 0)
  {
    TSVoiceSupFrame = &state->TS1SuperFrame;
  }
  else
  {
    TSVoiceSupFrame = &state->TS2SuperFrame;
  }

  CRCExtracted = 0;
  CRCComputed = 0;
  IrrecoverableErrors = 0;

  /* Deinterleave DMR data */
  BPTCDeInterleaveDMRData(info, DeInteleavedData);

  /* Extract the BPTC 196,96 DMR data */
  IrrecoverableErrors = BPTC_196x96_Extract_Data(DeInteleavedData, DmrDataBit, R);

  /* Fill the reserved bit (R(0)-R(2) of the BPTC(196,96) block) */
  BPTCReservedBits = (R[0] & 0x01) | ((R[1] << 1) & 0x02) | ((R[2] << 2) & 0x08);

  //define our block and padding values
  uint8_t blocks  = state->data_header_blocks[slot] - 1; //subtract 1 for the relevant value in the calc below
  uint8_t padding = state->data_header_padding[slot];
  //shift data in the superframe up a block
  k = 0;
  for(i = 0; i < 12; i++)
  {
    state->dmr_12_rate_sf[slot][i] = state->dmr_12_rate_sf[slot][i+12];    //shift block 2 to block 1
    state->dmr_12_rate_sf[slot][i+12] = state->dmr_12_rate_sf[slot][i+24]; //shift block 3 to block 2
    state->dmr_12_rate_sf[slot][i+24] = state->dmr_12_rate_sf[slot][i+36]; //shift block 4 to block 3
    state->dmr_12_rate_sf[slot][i+36] = state->dmr_12_rate_sf[slot][i+48]; //shift block 5 to block 4
    state->dmr_12_rate_sf[slot][i+48] = state->dmr_12_rate_sf[slot][i+60]; //shift block 6 to block 5
    state->dmr_12_rate_sf[slot][i+60] = state->dmr_12_rate_sf[slot][i+72]; //shift block 7 to block 6
    DmrDataByte[i] = 0;
    for(j = 0; j < 8; j++)
    {
      DmrDataByte[i] = DmrDataByte[i] << 1;
      DmrDataByte[i] = DmrDataByte[i] | (DmrDataBit[k] & 0x01);
      k++;
    }

    //start loading new superframe at appropriate block count determined by the data header
    state->dmr_12_rate_sf[slot][i+(blocks*12)] = DmrDataByte[i]; //if 3 blocks, then start at position 36; 4 blocks at 48, 5 at
  }

  /* Fill the CRC extracted (before Reed-Solomon (12,9) FEC correction) */
  CRCExtracted = 0;
  for(i = 0; i < 24; i++)
  {
    CRCExtracted = CRCExtracted << 1;
    CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 72] & 1);
  }

  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  CRCExtracted = CRCExtracted ^ 0x969696; //does this mask get applied here though for PI?

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x969696);

  //test
  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    //fprintf (stderr, "\n(1/2 Rate CRC Okay)");
  }
  else if((IrrecoverableErrors == 0))
  {
    //fprintf (stderr, "\n(1/2 Rate FEC Okay)");
  }
  else
  {
    fprintf (stderr, "%s", KRED);
    fprintf (stderr, ("\n(1/2 Rate CRC Fail, FEC Fail)"));
    fprintf (stderr, "%s", KNRM);
  }

  /* Convert corrected 12 bytes into 96 bits */
  for(i = 0, j = 0; i < 12; i++, j+=8)
  {
    DmrDataBit[j + 0] = (DmrDataByte[i] >> 7) & 0x01;
    DmrDataBit[j + 1] = (DmrDataByte[i] >> 6) & 0x01;
    DmrDataBit[j + 2] = (DmrDataByte[i] >> 5) & 0x01;
    DmrDataBit[j + 3] = (DmrDataByte[i] >> 4) & 0x01;
    DmrDataBit[j + 4] = (DmrDataByte[i] >> 3) & 0x01;
    DmrDataBit[j + 5] = (DmrDataByte[i] >> 2) & 0x01;
    DmrDataBit[j + 6] = (DmrDataByte[i] >> 1) & 0x01;
    DmrDataBit[j + 7] = (DmrDataByte[i] >> 0) & 0x01;
  }

  //Start Polling the 1/2 Rate Super Frame for Data when byte 0 contains values
  //LRRP
  int message_legnth = 0;
  if ( (state->dmr_12_rate_sf[slot][0] & 0x7F) == 0x45) //Start LRRP now
  {

    //find user home directory and append directory and filename.
    FILE * pFile; //put this outside of the if statement?
    if (opts->lrrp_file_output == 1)
    {
      char * filename = "/lrrp.txt";
      char * home_dir = getenv("HOME");
      char * filepath = malloc(strlen(home_dir) + strlen(filename) + 1);
      strncpy (filepath, home_dir, strlen(home_dir) + 1);
      strncat (filepath, filename, strlen(filename) + 1);
      //end find user home directory and append directory and filename

      pFile = fopen (filepath, "a");

      //hard code filename override for those wanting to use other software
      //pFile = fopen("DSDPlus.LRRP", "a");

      //seperate these two values since they cause issues with garbage writing to files in some environments (cygwin)
      fprintf (pFile, "%s\t", getDateL() ); //current date, may find a way to only add this IF no included timestamp in LRRP data?
      fprintf (pFile, "%s\t", getTimeL() ); //current timestamp, may find a way to only add this IF no included timestamp in LRRP data?
      fprintf (pFile, "%08lld\t", state->dmr_lrrp_source[state->currentslot]); //source address from data header
    }


    fprintf (stderr, "%s ", KMAG);

    //go to number of octets minus padding and crc, confirmed data may need a second rule to skip Serial Numbers and Block CRCs
    for (short i = 12; i < ( ((blocks+1)*12) - (padding+4) ); i++)
    {

      if ( state->dmr_12_rate_sf[slot][i] == 0x0C) //Source and Destination info
      {
        fprintf (stderr, "\n         Source: ");
        fprintf (stderr, " %03d.%03d.%03d.%03d", (state->dmr_12_rate_sf[slot][i+0] & 0x3F), state->dmr_12_rate_sf[slot][i+1], state->dmr_12_rate_sf[slot][i+2], state->dmr_12_rate_sf[slot][i+3]); //strip first two bits off 1st byte
        fprintf (stderr, " [%08d]", (state->dmr_12_rate_sf[slot][i+1] <<16 ) + (state->dmr_12_rate_sf[slot][i+2] << 8) + state->dmr_12_rate_sf[slot][i+3] );
        fprintf (stderr, " - Port %05d ", (state->dmr_12_rate_sf[slot][i+8] << 8) + state->dmr_12_rate_sf[slot][i+9]);
        fprintf (stderr, "\n    Destination: ");
        fprintf (stderr, " %03d.%03d.%03d.%03d", (state->dmr_12_rate_sf[slot][i+4] & 0x3F), state->dmr_12_rate_sf[slot][i+5], state->dmr_12_rate_sf[slot][6], state->dmr_12_rate_sf[slot][i+7]); //strip first two bits off 4th byte??
        fprintf (stderr, " [%08d]", (state->dmr_12_rate_sf[slot][i+5] <<16 ) + (state->dmr_12_rate_sf[slot][i+6] << 8) + state->dmr_12_rate_sf[slot][i+7] );
        fprintf (stderr, " - Port %05d", (state->dmr_12_rate_sf[slot][i+10] << 8) + state->dmr_12_rate_sf[slot][i+11]);

        i = i + 12; //skip 12 bytes here so we don't accidentally trip another flag with data in these bytes
      }

      if (state->dmr_12_rate_sf[slot][i] == 0x0D) //This should lock on to the second 0x0D value hopefully and tell us the message legnth
      {
        message_legnth = state->dmr_12_rate_sf[slot][i+1];
        fprintf (stderr, "\n Message Legnth: %02d", message_legnth);
        i = i + 2; //skip 2 bytes here so we don't accidentally trip another flag with data in these bytes
      }

      if ( state->dmr_12_rate_sf[slot][i] == 0x34 && message_legnth > 21 ) //timestamp, seems to need legnth of 27, shortened to 21 due to other samples
      {
        fprintf (stderr, "\n  LRRP - Timestamp: ");
        fprintf (stderr, "%4d.", (state->dmr_12_rate_sf[slot][i+1] << 6) + (state->dmr_12_rate_sf[slot][i+2] >> 2) ); //4 digit year
        fprintf (stderr, "%02d.", ( ((state->dmr_12_rate_sf[slot][i+2] & 0x3) << 2) + ((state->dmr_12_rate_sf[slot][i+3] & 0xC0) >> 6)) ); //2 digit month
        fprintf (stderr, "%02d",  ( (state->dmr_12_rate_sf[slot][i+3] & 0x30) >> 1) + ((state->dmr_12_rate_sf[slot][i+3] & 0x0E) >> 1)  ); //2 digit day
        fprintf (stderr, " %02d:",( (state->dmr_12_rate_sf[slot][i+3] & 0x01) << 4) + ((state->dmr_12_rate_sf[slot][i+4] & 0xF0) >> 4)  ); //2 digit hour
        fprintf (stderr,  "%02d:",( (state->dmr_12_rate_sf[slot][i+4] & 0x0F) << 2) + ((state->dmr_12_rate_sf[slot][i+5] & 0xC0) >> 6)  ); //2 digit minute
        fprintf (stderr,  "%02d", ( (state->dmr_12_rate_sf[slot][i+5] & 0x3F) << 0) );                                                     //2 digit second

        i = i + 6; //skip 6 bytes here so we don't accidentally trip another flag with data in these bytes
      }

      //is 66 a hemisphere or meridian difference from 0x51? Or different system type?
      if ( (state->dmr_12_rate_sf[slot][i] == 0x51 && message_legnth > 13) ||
           (state->dmr_12_rate_sf[slot][i] == 0x66 && message_legnth > 13)    ) //lattitude and longitude, message_lenght > 13?
      {
        fprintf (stderr, "\n  LRRP -");
        fprintf (stderr, " Lat: ");
        if (state->dmr_12_rate_sf[slot][i+1] & 0x80) //first bit indicates a sign, or hemisphere?
        {
          fprintf (stderr, "-");
          if (opts->lrrp_file_output == 1)
          {
            fprintf (pFile, "-");
          }
        }
        long int lrrplat;
        long int lrrplon;
        double lat_unit = (double)180/(double)4294967295;
        double lon_unit = (double)360/(double)4294967295;
        lrrplat = ( ( ((state->dmr_12_rate_sf[slot][i+1] & 0x7F ) <<  24 ) + (state->dmr_12_rate_sf[slot][i+2] << 16) + (state->dmr_12_rate_sf[slot][i+3] << 8) + state->dmr_12_rate_sf[slot][i+4]) * 1 );
        lrrplon = ( ( (state->dmr_12_rate_sf[slot][i+5]           <<  24 ) + (state->dmr_12_rate_sf[slot][i+6] << 16) + (state->dmr_12_rate_sf[slot][i+7] << 8) + state->dmr_12_rate_sf[slot][i+8]) * 1 );
        fprintf (stderr, "%.5lf ", ((double)lrrplat) * lat_unit);
        if (opts->lrrp_file_output == 1)
        {
          fprintf (pFile, "%.5lf\t", ((double)lrrplat) * lat_unit);
        }
        fprintf (stderr, " Lon: ");
        if (state->dmr_12_rate_sf[slot][i+5] & 0x80) //first bit indicates a sign, or hemisphere?
        {
          //fprintf (stderr, "-");
        }
        fprintf (stderr, "%.5lf", (lrrplon * lon_unit) );
        if (opts->lrrp_file_output == 1)
        {
          fprintf (pFile, "%.5lf\t", (lrrplon * lon_unit) );
        }
        if (state->dmr_12_rate_sf[slot][i+1] & 0x80) //first bit indicates a sign, or hemisphere?
        {
          sprintf ( state->dmr_lrrp[state->currentslot][3], "Lat: -%.5lf Lon: %.5lf ", ((double)lrrplat) * lat_unit , (lrrplon * lon_unit) );
        }
        else sprintf ( state->dmr_lrrp[state->currentslot][3], "Lat: %.5lf Lon: %.5lf ", ((double)lrrplat) * lat_unit , (lrrplon * lon_unit) );        //print for easy copy/paste into browser?
        //fprintf (stderr, " (");
        if (state->dmr_12_rate_sf[slot][i+1] & 0x80) //first bit indicates a sign, or hemisphere?
        {
          fprintf (stderr, " (-%.5lf, %.5lf)", ((double)lrrplat) * lat_unit , (lrrplon * lon_unit) );
        }
        else fprintf (stderr, " (%.5lf, %.5lf)", ((double)lrrplat) * lat_unit , (lrrplon * lon_unit) );

        i = i + 9 + 2; //skip 9 bytes here so we don't accidentally trip another flag with data in these bytes
      }


      if ( state->dmr_12_rate_sf[slot][i] == 0x6C && message_legnth > 13)
      {
        //either Plus is wrong, or I'm wrong on higher velocities exceeding 0xFF. double check double values?
        //fprintf (stderr, "\n  LRRP - Vi %02X Vf %02X Velocity Units (hex)", state->dmr_34_rate_sf[i+1], state->dmr_34_rate_sf[i+2]);
        double velocity = ( ((double)( (state->dmr_12_rate_sf[slot][i+1] ) + state->dmr_12_rate_sf[slot][i+2] )) / ( (double)128));
        if (opts->lrrp_file_output == 1)
        {
          fprintf (pFile, "%.3lf\t ", (velocity * 3.6) );
        }
        fprintf (stderr, "\n  LRRP - Velocity: %.4lf m/s %.4lf km/h %.4lf mph", velocity, (3.6 * velocity), (2.2369 * velocity) );
        sprintf ( state->dmr_lrrp[state->currentslot][4], "Vel: %.4lf kph ", (3.6 * velocity));

        i = i + 3; //skip 3 bytes here so we don't accidentally trip another flag with data in these bytes
      }
      if ( state->dmr_12_rate_sf[slot][i] == 0x56 && message_legnth > 13)
      {
        //check for appropriate terminology here - Heading, bearing, course, or track?
        if (opts->lrrp_file_output == 1)
        {
          fprintf (pFile, "%d\t", state->dmr_12_rate_sf[slot][i+1] * 2);
        }
        fprintf (stderr, "\n  LRRP - Track: %d Degrees", state->dmr_12_rate_sf[slot][i+1] * 2);
        sprintf ( state->dmr_lrrp[state->currentslot][5], "Dir: %d Deg ", state->dmr_12_rate_sf[slot][i+1] * 2);

        i = ((blocks+1)*12); //skip to end of loop here so we don't accidentally trip another flag with data in these bytes
      }
      //try for a Control ACK at the very end if nothing else pops, flag still unknown
      if ( (state->dmr_12_rate_sf[slot][i] == 0x36 || state->dmr_12_rate_sf[slot][i] == 0x37) && message_legnth > 6 )
      {
        fprintf (stderr, "\n  LRRP - Control ACK ");
      }
    }
    if (opts->lrrp_file_output == 1)
    {
      fprintf (pFile, "\n");
      fclose (pFile);
    }
  }
  fprintf (stderr, "%s ", KNRM);

  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr,"%s", KCYN);
    fprintf (stderr, "\n Full 1/2 Rate Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
    fprintf (stderr, "\n  Hex to Ascii - ");
    for (i = 0; i < 12; i++)
    {
      if (DmrDataByte[i] <= 0x7E && DmrDataByte[i] >=0x20)
      {
        fprintf (stderr, "%c", DmrDataByte[i]);
      }
      else fprintf (stderr, ".");
    }
    fprintf (stderr,"%s", KNRM);
  }
  //Full Super Frame - Debug Output
  if (opts->payload == 1 && state->data_block_counter[state->currentslot] == state->data_header_blocks[state->currentslot])
  {
    fprintf (stderr, "%s",KGRN);
    fprintf (stderr, "\n Rate 1/2 Superframe - Slot [%d]\n  ",slot+1);
    for (i = 0; i < ((blocks+1)*12); i++) //only print as many as we have blocks
    {
      fprintf (stderr, "[%02X]", state->dmr_12_rate_sf[slot][i]);
      if (i == 11 || i == 23 || i == 35 || i == 47 || i == 59 || i == 71) //line break and two spaces after each 12 bytes
      {
        fprintf (stderr, "\n  ");
      }
    }
    fprintf (stderr, "%s ", KNRM);
  }
  state->data_block_counter[state->currentslot]++; //increment block counter

}

void ProcessCSBK(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{

  uint32_t i, j, k;
  uint32_t CRCExtracted     = 0;
  uint32_t CRCComputed      = 0;
  uint32_t CRCCorrect       = 0;
  uint32_t IrrecoverableErrors = 0;
  uint8_t  DeInteleavedData[196];
  uint8_t  DmrDataBit[96];
  uint8_t  DmrDataByte[12];
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrame = NULL;
  uint8_t  R[3];
  uint8_t  BPTCReservedBits = 0;

  // Extract parameters for logging purposes
  uint8_t  csbk_lb   = 0;
  uint8_t  csbk_pf   = 0;
  uint8_t  csbk_o    = 0;
  uint8_t  csbk_fid  = 0;
  uint64_t csbk_data = 0;
  uint8_t  csbk      = 0;

  /* Check the current time slot */
  if(state->currentslot == 0)
  {
    TSVoiceSupFrame = &state->TS1SuperFrame;
  }
  else
  {
    TSVoiceSupFrame = &state->TS2SuperFrame;
  }

  CRCExtracted = 0;
  CRCComputed = 0;
  IrrecoverableErrors = 0;

  /* Deinterleave DMR data */
  BPTCDeInterleaveDMRData(info, DeInteleavedData);

  /* Extract the BPTC 196,96 DMR data */
  IrrecoverableErrors = BPTC_196x96_Extract_Data(DeInteleavedData, DmrDataBit, R);

  /* Fill the reserved bit (R(0)-R(2) of the BPTC(196,96) block) */
  BPTCReservedBits = (R[0] & 0x01) | ((R[1] << 1) & 0x02) | ((R[2] << 2) & 0x08);

  /* Convert the 96 bit of voice LC Header data into 12 bytes */
  k = 0;
  for(i = 0; i < 12; i++)
  {
    DmrDataByte[i] = 0;
    for(j = 0; j < 8; j++)
    {
      DmrDataByte[i] = DmrDataByte[i] << 1;
      DmrDataByte[i] = DmrDataByte[i] | (DmrDataBit[k] & 0x01);
      k++;
    }
  }

  /* Fill the CRC extracted (before Reed-Solomon (12,9) FEC correction) */
  CRCExtracted = 0;
  //for(i = 0; i < 24; i++)
  for(i = 0; i < 16; i++)
  {
    CRCExtracted = CRCExtracted << 1;
    //CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 72] & 1);
    CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 80] & 1); //80-96 for PI header
  }

  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  CRCExtracted = CRCExtracted ^ 0xA5A5;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0xA5A5);

  //test
  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    //fprintf (stderr, "\n(CSBK CRC Okay)");
  }
  else if((IrrecoverableErrors == 0))
  {
    //fprintf (stderr, "\n(CSBK FEC Okay)");
  }
  else
  {
    //fprintf (stderr, "%s", KRED);
    //fprintf (stderr, ("\n(CSBK CRC Fail, FEC Fail)"));
    //fprintf (stderr, "%s", KNRM);
  }

  /* Convert corrected 12 bytes into 96 bits */
  for(i = 0, j = 0; i < 12; i++, j+=8)
  {
    DmrDataBit[j + 0] = (DmrDataByte[i] >> 7) & 0x01;
    DmrDataBit[j + 1] = (DmrDataByte[i] >> 6) & 0x01;
    DmrDataBit[j + 2] = (DmrDataByte[i] >> 5) & 0x01;
    DmrDataBit[j + 3] = (DmrDataByte[i] >> 4) & 0x01;
    DmrDataBit[j + 4] = (DmrDataByte[i] >> 3) & 0x01;
    DmrDataBit[j + 5] = (DmrDataByte[i] >> 2) & 0x01;
    DmrDataBit[j + 6] = (DmrDataByte[i] >> 1) & 0x01;
    DmrDataBit[j + 7] = (DmrDataByte[i] >> 0) & 0x01;
  }

  csbk_lb  = ( (DmrDataByte[0] & 0x80) >> 7 );
  csbk_pf  = ( (DmrDataByte[0] & 0x40) >> 6 );
  csbk_o   =    DmrDataByte[0] & 0x3F; //grab 6 bits out of first Byte
  csbk_fid =    DmrDataByte[1];
  csbk_pf  = ( (DmrDataByte[1] & 0x80) >> 7);

  if ( ((csbk_o << 8) +  csbk_fid) == 0x0106 ) //are the FID values attached really necessary
  {
    uint8_t nb1 = DmrDataByte[2] & 0x3F; //extract(csbk, 18, 24); 3F or FD?
    uint8_t nb2 = DmrDataByte[3] & 0x3F; //extract(csbk, 26, 32);
    uint8_t nb3 = DmrDataByte[4] & 0x3F; //extract(csbk, 34, 40);
    uint8_t nb4 = DmrDataByte[5] & 0x3F; //extract(csbk, 42, 48);
    uint8_t nb5 = DmrDataByte[6] & 0x3F; //extract(csbk, 50, 56);
    fprintf (stderr, "%s", KMAG);
    fprintf (stderr, "\nMotoTRBO Connect Plus Neighbors\n");
    fprintf(stderr, " NB1(%02x), NB2(%02x), NB3(%02x), NB4(%02x), NB5(%02x)", nb1, nb2, nb3, nb4, nb5);
    fprintf (stderr, "%s", KNRM);
    sprintf(state->dmr_branding, " MotoTRBO Connect Plus ");
  }
  //ConnectPLUS Section //
  if ( ((csbk_o << 8) +  csbk_fid) == 0x0306 ) //are the FID values attached really necessary
  {
    uint32_t srcAddr = ( (DmrDataByte[2] << 16) + (DmrDataByte[3] << 8) + DmrDataByte[4] ); //extract(csbk, 16, 40);
    uint32_t grpAddr = ( (DmrDataByte[5] << 16) + (DmrDataByte[6] << 8) + DmrDataByte[7] ); //extract(csbk, 40, 64);
    uint8_t  lcn     = ( (DmrDataByte[8] & 0xF0 ) >> 4 ) ; //extract(csbk, 64, 68); 0xF0 LCN not report same as 'plus'?
    uint8_t  tslot   = ( (DmrDataByte[8] & 0x08 ) >> 3 ); //csbk[68]; TS seems fine
    fprintf (stderr, "%s", KGRN);
    fprintf (stderr, "\nMotoTRBO Connect Plus Channel Grant\n");
    fprintf(stderr, " srcAddr(%8d), grpAddr(%8d), LCN(%d), TS(%d)",srcAddr, grpAddr, lcn, tslot);
    fprintf (stderr, "%s", KNRM);
    sprintf(state->dmr_branding, " MotoTRBO Connect Plus ");

  }
  if ( ((csbk_o << 8) +  csbk_fid) == 0x0C06 ) //are the FID values attached really necessary?
  {
    uint32_t srcAddr = ( (DmrDataByte[2] << 16) + (DmrDataByte[3] << 8) + DmrDataByte[4] ); //extract(csbk, 16, 40);
    uint32_t grpAddr = ( (DmrDataByte[5] << 16) + (DmrDataByte[6] << 8) + DmrDataByte[7] ); //extract(csbk, 40, 64);
    uint8_t  lcn     = ( (DmrDataByte[8] & 0xF0 ) >> 4 ) ; //extract(csbk, 64, 68); 0xF0 LCN not report same as 'plus'?
    uint8_t  tslot   = ( (DmrDataByte[8] & 0x08 ) >> 3 ); //csbk[68]; TS seems fine
    fprintf (stderr, "%s", KRED);
    fprintf (stderr, "\nMotoTRBO Connect Plus Terminate Channel Grant\n"); //Data only shows a srcAddr??
    fprintf(stderr, " srcAddr(%8d), grpAddr(%8d), LCN(%d), TS(%d)",srcAddr, grpAddr, lcn, tslot);
    fprintf (stderr, "%s", KNRM);
    sprintf(state->dmr_branding, " MotoTRBO Connect Plus ");
  }

  if ( ((csbk_o << 8) +  csbk_fid) == 0x0606 ) //are the FID values attached really necessary?
  {
    uint32_t srcAddr = ( (DmrDataByte[2] << 16) + (DmrDataByte[3] << 8) + DmrDataByte[4] ); //extract(csbk, 16, 40);
    uint32_t grpAddr = ( (DmrDataByte[5] << 16) + (DmrDataByte[6] << 8) + DmrDataByte[7] ); //extract(csbk, 40, 64);
    uint8_t  lcn     = ( (DmrDataByte[8] & 0xF0 ) >> 4 ) ; //extract(csbk, 64, 68); 0xF0 LCN not report same as 'plus'?
    uint8_t  tslot   = ( (DmrDataByte[8] & 0x08 ) >> 3 ); //csbk[68]; TS seems fine
    fprintf (stderr, "%s", KCYN);
    fprintf (stderr, "\nMotoTRBO Connect Plus Data Channel Grant\n"); //Data I have doesn't show any values for lcn or tslot
    fprintf(stderr, " srcAddr(%8d), grpAddr(%8d), LCN(%d), TS(%d)",srcAddr, grpAddr, lcn, tslot);
    fprintf (stderr, "%s", KNRM);
    sprintf(state->dmr_branding, " MotoTRBO Connect Plus ");
  }

  if ( ((csbk_o << 8) +  csbk_fid) == 0x1106 ) //are the FID values attached really necessary?
  {
    fprintf (stderr, "\nMotoTRBO Connect Plus Registration Request");
    sprintf(state->dmr_branding, " MotoTRBO Connect Plus ");
  }

  if ( ((csbk_o << 8) +  csbk_fid) == 0x1206 ) //are the FID values attached really necessary?
  {
    fprintf (stderr, "\nMotoTRBO Connect Plus Registration Response");
    sprintf(state->dmr_branding, " MotoTRBO Connect Plus ");
  }

  if ( ((csbk_o << 8) +  csbk_fid) == 0x1806 ) //are the FID values attached really necessary?
  {
    fprintf (stderr, "\nMotoTRBO Connect Plus Talkgroup Affiliation");
    sprintf(state->dmr_branding, " MotoTRBO Connect Plus ");
  }

  //CapMAX
  if ( ((csbk_o << 8) +  csbk_fid) == 0x1906 ) //CapMAX Aloha
  {
    //fprintf (stderr, "\nCapacity Max Aloha!");
    sprintf(state->dmr_branding, " MotoTRBO Capacity Max ");
  }

  //CapacityPlus Section

  //CapMAX
  //if ( ((csbk_o << 8) +  csbk_fid) == 0x1F06 ) //
  if (csbk_o == 0x1F)
  {
    uint32_t srcAddr = ( (DmrDataByte[2] << 16) + (DmrDataByte[3] << 8) + DmrDataByte[4] ); //extract(csbk, 16, 40);
    uint32_t grpAddr = ( (DmrDataByte[5] << 16) + (DmrDataByte[6] << 8) + DmrDataByte[7] ); //extract(csbk, 40, 64);
    uint8_t  lcn     = ( (DmrDataByte[8] & 0xF0 ) >> 4 ) ; //extract(csbk, 64, 68); 0xF0 LCN not report same as 'plus'?
    uint8_t  tslot   = ( (DmrDataByte[8] & 0x08 ) >> 3 ); //csbk[68]; TS seems fine
    fprintf (stderr, "\nMoto Capacity Plus Call Alert");
    //fprintf(stderr, " srcAddr(%8d), grpAddr(%8d), LCN(%d), TS(%d)",srcAddr, grpAddr, lcn, tslot);
    sprintf(state->dmr_branding, " MotoTRBO Capacity Plus ");
  }

  //if ( ((csbk_o << 8) +  csbk_fid) == 0x2006 ) //
  if (csbk_o == 0x20)
  {
    uint32_t srcAddr = ( (DmrDataByte[2] << 16) + (DmrDataByte[3] << 8) + DmrDataByte[4] ); //extract(csbk, 16, 40);
    uint32_t grpAddr = ( (DmrDataByte[5] << 16) + (DmrDataByte[6] << 8) + DmrDataByte[7] ); //extract(csbk, 40, 64);
    uint8_t  lcn     = ( (DmrDataByte[8] & 0xF0 ) >> 4 ) ; //extract(csbk, 64, 68); 0xF0 LCN not report same as 'plus'?
    uint8_t  tslot   = ( (DmrDataByte[8] & 0x08 ) >> 3 ); //csbk[68]; TS seems fine
    fprintf (stderr, "\nMotoTRBO Capacity Max Call Alert Ack");
    //fprintf(stderr, " srcAddr(%8d), grpAddr(%8d), LCN(%d), TS(%d)",srcAddr, grpAddr, lcn, tslot);
    sprintf(state->dmr_branding, " MotoTRBO Capacity Plus ");
  }

  //if ( ((csbk_o << 8) +  csbk_fid) == 0x3B06 ) //are the FID values attached really necessary
  if (csbk_o == 0x3B)
  {
    uint8_t nb1 = DmrDataByte[2] & 0x3F; //extract(csbk, 18, 24); 3F or FD?
    uint8_t nb2 = DmrDataByte[3] & 0x3F; //extract(csbk, 26, 32);
    uint8_t nb3 = DmrDataByte[4] & 0x3F; //extract(csbk, 34, 40);
    uint8_t nb4 = DmrDataByte[5] & 0x3F; //extract(csbk, 42, 48);
    uint8_t nb5 = DmrDataByte[6] & 0x3F; //extract(csbk, 50, 56);
    fprintf (stderr, "\nMotoTRBO Capacity Plus Neighbors");
    //fprintf(stderr, " NB1(%02x), NB2(%02x), NB3(%02x), NB4(%02x), NB5(%02x)", nb1, nb2, nb3, nb4, nb5);
    sprintf(state->dmr_branding, " MotoTRBO Capacity Plus ");
  }

  //if ( ((csbk_o << 8) +  csbk_fid) == 0x3D06 ) //
  if (csbk_o == 0x3D)
  {
    //fprintf (stderr, "\nCapacity Plus/Hytera Preamble");
    //sprintf(state->dmr_branding, " MotoTRBO Capacity Plus ");
    //sprintf(state->dmr_branding, " TIII "); //?? one of these next two seems to be on both types, maybe its a TIII thing?
  }

  //if ( ((csbk_o << 8) +  csbk_fid) == 0x3E06 ) //
  if (csbk_o == 0x3E)
  {
    //fprintf (stderr, "\nCapacity Plus System Status");
    //sprintf(state->dmr_branding, " MotoTRBO Capacity Plus ");
    //sprintf(state->dmr_branding, " TIII "); //?? one of these next two seems to be on both types, maybe its a TIII thing?
  }

  //possible Site identifier, CSBK Aloha?
  if (csbk_o == 0x19) //DmrDataByte[0] == 0x99?
  {
    //fprintf (stderr, "\n  CSBK Aloha?");
    //fprintf (stderr, "\n  Site ID %d-%d.%d", (DmrDataByte[2] & 0xF0 >> 4), DmrDataByte[2] & 0xF, DmrDataByte[3]);
    //fprintf (stderr, "  DCC %d", DmrDataByte[1]);
    //sprintf ( state->dmr_callsign[0], "Site ID %d-%d.%d", (DmrDataByte[2] & 0xF0 >> 4), DmrDataByte[2] & 0xF, DmrDataByte[3]);
    //sprintf(state->dmr_branding, " TIII "); //?? one of these next two seems to be on both types, maybe its a TIII thing?
  }


  //Hytera XPT
  if (csbk_o == 0x28)
  {
    //fprintf (stderr, "\nHytera TIII Announcement");
    //sprintf(state->dmr_branding, " MotoTRBO Capacity Plus ");
    sprintf(state->dmr_branding, " TIII "); //?? one of these next two seems to be on both types, maybe its a TIII thing?
  }
  //if ( ((csbk_o << 8) +  csbk_fid) == 0x3606 ) //
  if (csbk_o == 0x36)
  {
    //fprintf (stderr, "\nHytera XPT");
    sprintf(state->dmr_branding, " Hytera XPT ");
  }

  if (csbk_o == 0x0A)
  {
    //fprintf (stderr, "\nHytera XPT Site State");
    sprintf(state->dmr_branding, " Hytera XPT ");
  }

  //if ( ((csbk_o << 8) +  csbk_fid) == 0x3706 ) //
  if (csbk_o == 0x37)
  {
    //fprintf (stderr, "\nHytera XPT");
    sprintf(state->dmr_branding, " Hytera XPT ");
  }

  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "%s", KCYN);
    fprintf (stderr, "\nFull CSBK Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
    fprintf (stderr, "%s", KNRM);
  }

}

void ProcessDmrPIHeader(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{

  uint32_t i, j, k;
  uint32_t CRCExtracted     = 0;
  uint32_t CRCComputed      = 0;
  uint32_t CRCCorrect       = 0;
  uint32_t IrrecoverableErrors = 0;
  uint8_t  DeInteleavedData[196];
  uint8_t  DmrDataBit[96];
  uint8_t  DmrDataByte[12];
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrame = NULL;
  uint8_t  R[3];
  uint8_t  BPTCReservedBits = 0;

  /* Check the current time slot */
  if(state->currentslot == 0)
  {
    TSVoiceSupFrame = &state->TS1SuperFrame;
  }
  else
  {
    TSVoiceSupFrame = &state->TS2SuperFrame;
  }

  CRCExtracted = 0;
  CRCComputed = 0;
  IrrecoverableErrors = 0;

  /* Deinterleave DMR data */
  BPTCDeInterleaveDMRData(info, DeInteleavedData);

  /* Extract the BPTC 196,96 DMR data */
  IrrecoverableErrors = BPTC_196x96_Extract_Data(DeInteleavedData, DmrDataBit, R);

  /* Fill the reserved bit (R(0)-R(2) of the BPTC(196,96) block) */
  BPTCReservedBits = (R[0] & 0x01) | ((R[1] << 1) & 0x02) | ((R[2] << 2) & 0x08);

  /* Convert the 96 bit of voice LC Header data into 12 bytes */
  k = 0;
  for(i = 0; i < 12; i++)
  {
    DmrDataByte[i] = 0;
    for(j = 0; j < 8; j++)
    {
      DmrDataByte[i] = DmrDataByte[i] << 1;
      DmrDataByte[i] = DmrDataByte[i] | (DmrDataBit[k] & 0x01);
      k++;
    }
  }

  /* Fill the CRC extracted (before Reed-Solomon (12,9) FEC correction) */
  CRCExtracted = 0;
  //for(i = 0; i < 24; i++)
  for(i = 0; i < 16; i++)
  {
    CRCExtracted = CRCExtracted << 1;
    //CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 72] & 1);
    CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 80] & 1); //80-96 for PI header
  }

  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  CRCExtracted = CRCExtracted ^ 0x6969;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x6969);

  /* Convert corrected 12 bytes into 96 bits */
  for(i = 0, j = 0; i < 12; i++, j+=8)
  {
    DmrDataBit[j + 0] = (DmrDataByte[i] >> 7) & 0x01;
    DmrDataBit[j + 1] = (DmrDataByte[i] >> 6) & 0x01;
    DmrDataBit[j + 2] = (DmrDataByte[i] >> 5) & 0x01;
    DmrDataBit[j + 3] = (DmrDataByte[i] >> 4) & 0x01;
    DmrDataBit[j + 4] = (DmrDataByte[i] >> 3) & 0x01;
    DmrDataBit[j + 5] = (DmrDataByte[i] >> 2) & 0x01;
    DmrDataBit[j + 6] = (DmrDataByte[i] >> 1) & 0x01;
    DmrDataBit[j + 7] = (DmrDataByte[i] >> 0) & 0x01;
  }

  if (state->currentslot == 0)
  {
    state->payload_algid = DmrDataByte[0];
    state->payload_keyid = DmrDataByte[2];
    state->payload_mi    = ( ((DmrDataByte[3]) << 24) + ((DmrDataByte[4]) << 16) + ((DmrDataByte[5]) << 8) + (DmrDataByte[6]) );
    //state->payload_mi = 0; //testing for late entry, remove later
    if (state->payload_algid < 0x26) //have it always print? only if a good value, was 1 == 1
    {
      fprintf (stderr, "%s ", KYEL);
      fprintf (stderr, "\n Slot 1");
      fprintf (stderr, " DMR PI Header ALG ID: 0x%02X KEY ID: 0x%02X MI: 0x%08X", state->payload_algid, state->payload_keyid, state->payload_mi);
      fprintf (stderr, "%s ", KNRM);
    }
    if (state->payload_algid >= 0x26) //sanity check to make sure we aren't keeping bogus PI header info
    {
      state->payload_algid = 0;
      state->payload_keyid = 0;
      state->payload_mi = 0;
    }
  }

  if (state->currentslot == 1)
  {
    state->payload_algidR = DmrDataByte[0];
    state->payload_keyidR = DmrDataByte[2];
    state->payload_miR    = ( ((DmrDataByte[3]) << 24) + ((DmrDataByte[4]) << 16) + ((DmrDataByte[5]) << 8) + (DmrDataByte[6]) );
    //state->payload_miR = 0; //testing for late entry, remove later
    if (state->payload_algidR < 0x26) //have it always print? only if a good value, was 1 == 1
    {
      fprintf (stderr, "%s ", KYEL);
      fprintf (stderr, "\n Slot 2");
      fprintf (stderr, " DMR PI Header ALG ID: 0x%02X KEY ID: 0x%02X MI: 0x%08X", state->payload_algidR, state->payload_keyidR, state->payload_miR);
      fprintf (stderr, "%s ", KNRM);
    }
    if (state->payload_algidR >= 0x26) //sanity check to make sure we aren't keeping bogus PI header info
    {
      state->payload_algidR = 0;
      state->payload_keyidR = 0;
      state->payload_miR = 0;
    }
  }


  //test
  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    //fprintf (stderr, " (PI CRC Okay)");
  }
  else if((IrrecoverableErrors == 0))
  {
    //fprintf (stderr, " (PI FEC Okay)");
  }
  else {
    fprintf (stderr, "%s ", KRED);
    fprintf (stderr, ("\n (PI CRC Fail, FEC Fail)"));
    fprintf (stderr, "%s ", KNRM);
  }

  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "%s", KCYN);
    fprintf (stderr, "\nFull PI Header Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
    fprintf (stderr, "%s", KNRM);
  }

}

void ProcessDmrVoiceLcHeader(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{
  uint32_t i, j, k;
  uint32_t CRCExtracted     = 0;
  uint32_t CRCComputed      = 0;
  uint32_t CRCCorrect       = 0;
  uint32_t IrrecoverableErrors = 0;
  uint8_t  DeInteleavedData[196];
  uint8_t  DmrDataBit[96];
  uint8_t  DmrDataByte[12];
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrame = NULL;
  uint8_t  R[3];
  uint8_t  BPTCReservedBits = 0;

  /* Check the current time slot */
  if(state->currentslot == 0)
  {
    TSVoiceSupFrame = &state->TS1SuperFrame;
  }
  else
  {
    TSVoiceSupFrame = &state->TS2SuperFrame;
  }

  CRCExtracted = 0;
  CRCComputed = 0;
  IrrecoverableErrors = 0;

  /* Deinterleave DMR data */
  BPTCDeInterleaveDMRData(info, DeInteleavedData);

  /* Extract the BPTC 196,96 DMR data */
  IrrecoverableErrors = BPTC_196x96_Extract_Data(DeInteleavedData, DmrDataBit, R);

  /* Fill the reserved bit (R(0)-R(2) of the BPTC(196,96) block) */
  BPTCReservedBits = (R[0] & 0x01) | ((R[1] << 1) & 0x02) | ((R[2] << 2) & 0x08);

  /* Convert the 96 bit of voice LC Header data into 12 bytes */
  k = 0;
  for(i = 0; i < 12; i++)
  {
    DmrDataByte[i] = 0;
    for(j = 0; j < 8; j++)
    {
      DmrDataByte[i] = DmrDataByte[i] << 1;
      DmrDataByte[i] = DmrDataByte[i] | (DmrDataBit[k] & 0x01);
      k++;
    }
  }

  /* Fill the CRC extracted (before Reed-Solomon (12,9) FEC correction) */
  CRCExtracted = 0;
  for(i = 0; i < 24; i++)
  {
    CRCExtracted = CRCExtracted << 1;
    CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 72] & 1);
  }

  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  CRCExtracted = CRCExtracted ^ 0x969696;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x969696);

  /* Convert corrected 12 bytes into 96 bits */
  for(i = 0, j = 0; i < 12; i++, j+=8)
  {
    DmrDataBit[j + 0] = (DmrDataByte[i] >> 7) & 0x01;
    DmrDataBit[j + 1] = (DmrDataByte[i] >> 6) & 0x01;
    DmrDataBit[j + 2] = (DmrDataByte[i] >> 5) & 0x01;
    DmrDataBit[j + 3] = (DmrDataByte[i] >> 4) & 0x01;
    DmrDataBit[j + 4] = (DmrDataByte[i] >> 3) & 0x01;
    DmrDataBit[j + 5] = (DmrDataByte[i] >> 2) & 0x01;
    DmrDataBit[j + 6] = (DmrDataByte[i] >> 1) & 0x01;
    DmrDataBit[j + 7] = (DmrDataByte[i] >> 0) & 0x01;
  }
  //fprintf (stderr, "\nDDB = 0x%X \n", DmrDataBit);
  /* Store the Protect Flag (PF) bit */
  TSVoiceSupFrame->FullLC.ProtectFlag = (unsigned int)(DmrDataBit[0]);

  /* Store the Reserved bit */
  TSVoiceSupFrame->FullLC.Reserved = (unsigned int)(DmrDataBit[1]);

  /* Store the Full Link Control Opcode (FLCO)  */
  TSVoiceSupFrame->FullLC.FullLinkControlOpcode = (unsigned int)ConvertBitIntoBytes(&DmrDataBit[2], 6);

  /* Store the Feature set ID (FID)  */
  TSVoiceSupFrame->FullLC.FeatureSetID = (unsigned int)ConvertBitIntoBytes(&DmrDataBit[8], 8);
  //state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;

  /* Store the Service Options  */
  TSVoiceSupFrame->FullLC.ServiceOptions = (unsigned int)ConvertBitIntoBytes(&DmrDataBit[16], 8);

  /* Store the Group address (Talk Group) */
  TSVoiceSupFrame->FullLC.GroupAddress = (unsigned int)ConvertBitIntoBytes(&DmrDataBit[24], 24);

  /* Store the Source address */
  TSVoiceSupFrame->FullLC.SourceAddress = (unsigned int)ConvertBitIntoBytes(&DmrDataBit[48], 24);
  //state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;

  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    /* CRC is correct so consider the Full LC data as correct/valid */
    TSVoiceSupFrame->FullLC.DataValidity = 1;
    if (state->currentslot == 0)
    {
      state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
    }
    if (state->currentslot == 1)
    {
      state->dmr_soR = TSVoiceSupFrame->FullLC.ServiceOptions;
    }
  }
  else if(IrrecoverableErrors == 0)
  {
    //FEC okay? Set SVCop code anyways
    TSVoiceSupFrame->FullLC.DataValidity = 0; //shouldn't matter in this context
    if (state->currentslot == 0)
    {
      state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
    }
    if (state->currentslot == 1)
    {
      state->dmr_soR = TSVoiceSupFrame->FullLC.ServiceOptions;
    }
  }
  else
  {
    /* CRC checking error, so consider the Full LC data as invalid */
    TSVoiceSupFrame->FullLC.DataValidity = 0;
  }
  //Full, amateur callsigns and labels not on this,
  if (opts->payload == 1)
  {
    //fprintf (stderr, "\nFull VoiceLC Payload ");
    for (i = 0; i < 12; i++)
    {
      //fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
  }
  /* Print the destination ID (TG) and the source ID */
  fprintf (stderr, "%s \n", KGRN);
  fprintf (stderr, " Slot %d ", state->currentslot+1);
  fprintf(stderr, "  TGT=%u  SRC=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);

  if(TSVoiceSupFrame->FullLC.ServiceOptions & 0x80) fprintf(stderr, "Emergency ");
  if(TSVoiceSupFrame->FullLC.ServiceOptions & 0x40)
  {
    fprintf (stderr, "%s ", KRED);
    fprintf(stderr, "Encrypted ");
    //fprintf (stderr, "%s ", KNRM);
  }
  else
  {
    fprintf (stderr, "%s ", KGRN);
    fprintf(stderr, "Clear/Unencrypted ");
    //fprintf (stderr, "%s ", KNRM);
  }

  /* Check the "Reserved" bits */
  if(TSVoiceSupFrame->FullLC.ServiceOptions & 0x30)
  {
    /* Experimentally determined with DSD+, when the "Reserved" bit field
     * is equal to 0x2, this is a TXI call */
    if((TSVoiceSupFrame->FullLC.ServiceOptions & 0x30) == 0x20) fprintf(stderr, "TXI ");
    else fprintf(stderr, "Reserved=%d ", (TSVoiceSupFrame->FullLC.ServiceOptions & 0x30) >> 4);
  }
  if(TSVoiceSupFrame->FullLC.ServiceOptions & 0x08) fprintf(stderr, "Broadcast ");
  if(TSVoiceSupFrame->FullLC.ServiceOptions & 0x04) fprintf(stderr, "OVCM ");
  if(TSVoiceSupFrame->FullLC.ServiceOptions & 0x03)
  {
    if((TSVoiceSupFrame->FullLC.ServiceOptions & 0x03) == 0x01) fprintf(stderr, "Priority 1 ");
    else if((TSVoiceSupFrame->FullLC.ServiceOptions & 0x03) == 0x02) fprintf(stderr, "Priority 2 ");
    else if((TSVoiceSupFrame->FullLC.ServiceOptions & 0x03) == 0x03) fprintf(stderr, "Priority 3 ");
    else fprintf(stderr, "No Priority "); /* We should never go here */
  }
  fprintf(stderr, "Call ");
  fprintf (stderr, "%s ", KNRM);

  if(TSVoiceSupFrame->FullLC.DataValidity) {}//fprintf(stderr, "(CRC OK )  ");
  else if(IrrecoverableErrors == 0) {}//fprintf(stderr, "RAS (FEC OK/CRC ERR)");
  else {}//fprintf(stderr, "(FEC FAIL/CRC ERR)");

#ifdef PRINT_VOICE_LC_HEADER_BYTES
  fprintf(stderr, "\n");
  fprintf(stderr, "VOICE LC HEADER : ");
  for(i = 0; i < 12; i++)
  {
    fprintf(stderr, "0x%02X", DmrDataByte[i]);
    if(i != 11) fprintf(stderr, " - ");
  }

  fprintf(stderr, "\n");

  fprintf(stderr, "BPTC(196,96) Reserved bit R(0)-R(2) = 0x%02X\n", BPTCReservedBits);

  fprintf(stderr, "CRC extracted = 0x%04X - CRC computed = 0x%04X - ", CRCExtracted, CRCComputed);

  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    fprintf(stderr, "CRCs are equal + FEC OK !\n");
  }
  else if(IrrecoverableErrors == 0)
  {
    else fprintf(stderr, "FEC correctly corrected but CRCs are incorrect\n");
  }
  else
  {
    fprintf(stderr, "ERROR !!! CRCs are different and FEC Failed !\n");
  }

  fprintf(stderr, "Hamming Irrecoverable Errors = %u\n", IrrecoverableErrors);
#endif /* PRINT_VOICE_LC_HEADER_BYTES */

//Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "%s", KCYN);
    fprintf (stderr, "\nFull Voice LC Header Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
    fprintf (stderr, "%s", KNRM);
  }

} /* End ProcessDmrVoiceLcHeader() */


void ProcessDmrTerminaisonLC(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{
  uint32_t i, j, k;
  uint32_t CRCExtracted     = 0;
  uint32_t CRCComputed      = 0;
  uint32_t CRCCorrect       = 0;
  uint32_t IrrecoverableErrors = 0;
  uint8_t  DeInteleavedData[196];
  uint8_t  DmrDataBit[96];
  uint8_t  DmrDataByte[12];
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrame = NULL;
  uint8_t  R[3];
  uint8_t  BPTCReservedBits = 0;

  /* Check the current time slot */
  if(state->currentslot == 0)
  {
    TSVoiceSupFrame = &state->TS1SuperFrame;
  }
  else
  {
    TSVoiceSupFrame = &state->TS2SuperFrame;
  }

  CRCExtracted = 0;
  CRCComputed = 0;
  IrrecoverableErrors = 0;

  /* Deinterleave DMR data */
  BPTCDeInterleaveDMRData(info, DeInteleavedData);

  /* Extract the BPTC 196,96 DMR data */
  IrrecoverableErrors = BPTC_196x96_Extract_Data(DeInteleavedData, DmrDataBit, R);

  /* Fill the reserved bit (R(0)-R(2) of the BPTC(196,96) block) */
  BPTCReservedBits = (R[0] & 0x01) | ((R[1] << 1) & 0x02) | ((R[2] << 2) & 0x08);

  /* Convert the 96 bit of voice LC Header data into 12 bytes */
  k = 0;
  for(i = 0; i < 12; i++)
  {
    DmrDataByte[i] = 0;
    for(j = 0; j < 8; j++)
    {
      DmrDataByte[i] = DmrDataByte[i] << 1;
      DmrDataByte[i] = DmrDataByte[i] | (DmrDataBit[k] & 0x01);
      k++;
    }
  }

  /* Fill the CRC extracted (before Reed-Solomon (12,9) FEC correction) */
  CRCExtracted = 0;
  for(i = 0; i < 24; i++)
  {
    CRCExtracted = CRCExtracted << 1;
    CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 72] & 1);
  }

  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  CRCExtracted = CRCExtracted ^ 0x999999;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x999999);

  /* Convert corrected 12 bytes into 96 bits */
  for(i = 0, j = 0; i < 12; i++, j+=8)
  {
    DmrDataBit[j + 0] = (DmrDataByte[i] >> 7) & 0x01;
    DmrDataBit[j + 1] = (DmrDataByte[i] >> 6) & 0x01;
    DmrDataBit[j + 2] = (DmrDataByte[i] >> 5) & 0x01;
    DmrDataBit[j + 3] = (DmrDataByte[i] >> 4) & 0x01;
    DmrDataBit[j + 4] = (DmrDataByte[i] >> 3) & 0x01;
    DmrDataBit[j + 5] = (DmrDataByte[i] >> 2) & 0x01;
    DmrDataBit[j + 6] = (DmrDataByte[i] >> 1) & 0x01;
    DmrDataBit[j + 7] = (DmrDataByte[i] >> 0) & 0x01;
  }
  //fprintf (stderr, "\nDDB = 0x%X \n", DmrDataBit);
  /* Store the Protect Flag (PF) bit */
  TSVoiceSupFrame->FullLC.ProtectFlag = (unsigned int)(DmrDataBit[0]);

  /* Store the Reserved bit */
  TSVoiceSupFrame->FullLC.Reserved = (unsigned int)(DmrDataBit[1]);

  /* Store the Full Link Control Opcode (FLCO)  */
  TSVoiceSupFrame->FullLC.FullLinkControlOpcode = (unsigned int)ConvertBitIntoBytes(&DmrDataBit[2], 6);

  /* Store the Feature set ID (FID)  */
  TSVoiceSupFrame->FullLC.FeatureSetID = (unsigned int)ConvertBitIntoBytes(&DmrDataBit[8], 8);
  //state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;

  /* Store the Service Options  */
  TSVoiceSupFrame->FullLC.ServiceOptions = (unsigned int)ConvertBitIntoBytes(&DmrDataBit[16], 8);

  /* Store the Group address (Talk Group) */
  TSVoiceSupFrame->FullLC.GroupAddress = (unsigned int)ConvertBitIntoBytes(&DmrDataBit[24], 24);

  /* Store the Source address */
  TSVoiceSupFrame->FullLC.SourceAddress = (unsigned int)ConvertBitIntoBytes(&DmrDataBit[48], 24);

  if((IrrecoverableErrors == 0))// && CRCCorrect)
  {
    /* CRC is correct so consider the Full LC data as correct/valid */
    TSVoiceSupFrame->FullLC.DataValidity = 1;
  }
  else
  {
    /* CRC checking error, so consider the Full LC data as invalid */
    TSVoiceSupFrame->FullLC.DataValidity = 0;
  }

  //reset alg, keys, mi during a TLC call termination EVENT so we aren't stuck on an old value
  if (state->currentslot == 0)
  {
    state->payload_algid = 0;
    state->payload_keyid = 0;
    //state->payload_mfid  = 0;
    state->payload_mi    = 0;

  }
  if (state->currentslot == 1)
  {
    state->payload_algidR = 0;
    state->payload_keyidR = 0;
    //state->payload_mfid  = 0;
    state->payload_miR    = 0;

  }

  //tlc
  if((IrrecoverableErrors == 0) && CRCCorrect) //amateur DMR seems to only set radio ID up here I think, figure out best way to set without messing up other DMR types
  {
    fprintf (stderr, "%s \n", KRED);
    fprintf (stderr, "  SLOT %d", state->currentslot+1);
    fprintf(stderr, " TGT=%u  SRC=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
    //fprintf(stderr, "FID=0x%02X ", TSVoiceSupFrame->FullLC.FeatureSetID);
    fprintf (stderr, "%s ", KNRM);
    //fprintf(stderr, "(CRC OK )  ");
    if (TSVoiceSupFrame->FullLC.FullLinkControlOpcode == 0) //other opcodes may convey callsigns, names, etc.
    {
      if (state->currentslot == 0)
      {
        state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
        state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
      }
      if (state->currentslot == 1)
      {
        state->dmr_fidR = TSVoiceSupFrame->FullLC.FeatureSetID;
        state->dmr_soR = TSVoiceSupFrame->FullLC.ServiceOptions;
      }
    }
  }
  else if(IrrecoverableErrors == 0)
  {
    fprintf (stderr, "%s \n", KRED);
    fprintf (stderr, "  SLOT %d", state->currentslot+1);
    fprintf(stderr, " TGT=%u  SRC=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
    //fprintf(stderr, "FID=0x%02X ", TSVoiceSupFrame->FullLC.FeatureSetID);
    fprintf(stderr, "RAS (FEC OK/CRC ERR)"); //tlc
    fprintf (stderr, "%s ", KNRM);
    if (TSVoiceSupFrame->FullLC.FullLinkControlOpcode == 0) //other opcodes may convey callsigns, names, etc.

    {
      if (state->currentslot == 0)
      {
        state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
        state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
      }
      if (state->currentslot == 1)
      {
        state->dmr_fidR = TSVoiceSupFrame->FullLC.FeatureSetID;
        state->dmr_soR = TSVoiceSupFrame->FullLC.ServiceOptions;
      }
    }
  }
  else {} //fprintf(stderr, "\n(FEC FAIL/CRC ERR)");

#ifdef PRINT_TERMINAISON_LC_BYTES
  fprintf(stderr, "\n");
  fprintf(stderr, "TERMINAISON LINK CONTROL (TLC) : ");
  for(i = 0; i < 12; i++)
  {
    fprintf(stderr, "0x%02X", DmrDataByte[i]);
    if(i != 11) fprintf(stderr, " - ");
  }

  fprintf(stderr, "\n");

  fprintf(stderr, "BPTC(196,96) Reserved bit R(0)-R(2) = 0x%02X\n", BPTCReservedBits);

  fprintf(stderr, "CRC extracted = 0x%04X - CRC computed = 0x%04X - ", CRCExtracted, CRCComputed);

  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    fprintf(stderr, "CRCs are equal + FEC OK !\n");
  }
  else if(IrrecoverableErrors == 0)
  {
    else fprintf(stderr, "FEC correctly corrected but CRCs are incorrect\n");
  }
  else
  {
    fprintf(stderr, "ERROR !!! CRCs are different and FEC Failed !\n");
  }

  fprintf(stderr, "Hamming Irrecoverable Errors = %u\n", IrrecoverableErrors);
#endif /* PRINT_TERMINAISON_LC_BYTES */

  if (opts->payload == 1)
  {
    fprintf (stderr, "%s", KCYN);
    fprintf (stderr, "\nFull TLC Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
    fprintf (stderr, "%s", KNRM);
  }
} /* End ProcessDmrTerminaisonLC() */


/* Extract the Link Control (LC) embedded in the SYNC
 * of a DMR voice superframe */
void ProcessVoiceBurstSync(dsd_opts * opts, dsd_state * state)
{
  uint32_t i, j, k;
  uint32_t Burst;
  uint8_t  BptcDataMatrix[8][16];
  uint8_t  LC_DataBit[77];
  uint8_t  LC_DataBytes[10];
  TimeSlotVoiceSuperFrame_t * TSVoiceSupFrame = NULL;
  uint32_t IrrecoverableErrors;
  uint8_t  CRCExtracted;
  uint8_t  CRCComputed;
  uint32_t CRCCorrect = 0;

  /* Remove warning compiler */
  //UNUSED_VARIABLE(opts);

  /* Check the current time slot */

  if(state->currentslot == 0)
  {
    TSVoiceSupFrame = &state->TS1SuperFrame;
  }
  else
  {
    TSVoiceSupFrame = &state->TS2SuperFrame;
  }


  /* First step : Reconstitute the BPTC 16x8 matrix */
  Burst = 1; /* Burst B to E contains embedded signaling data */
  k = 0;
  for(i = 0; i < 16; i++)
  {
    for(j = 0; j < 8; j++)
    {
      /* Only the LSBit of the byte is stored */
      BptcDataMatrix[j][i] = TSVoiceSupFrame->TimeSlotRawVoiceFrame[Burst].Sync[k + 8];
      k++;

      /* Go on to the next burst once 32 bit
       * of the SNYC have been stored */
      if(k >= 32)
      {
        k = 0;
        Burst++;
      }
    } /* End for(j = 0; j < 8; j++) */
  } /* End for(i = 0; i < 16; i++) */

  /* Extract the 72 LC bit (+ 5 CRC bit) of the matrix */
  IrrecoverableErrors = BPTC_128x77_Extract_Data(BptcDataMatrix, LC_DataBit);

  /* Convert the 77 bit of voice LC Header data into 9 bytes */
  k = 0;
  for(i = 0; i < 10; i++)
  {
    LC_DataBytes[i] = 0;
    for(j = 0; j < 8; j++)
    //for(j = 0; j < 10; j++)
    {
      LC_DataBytes[i] = LC_DataBytes[i] << 1;
      LC_DataBytes[i] = LC_DataBytes[i] | (LC_DataBit[k] & 0x01);
      k++;
      if(k >= 76) break;
      //if(k >= 80) break;
    }
  }

  /* Reconstitute the 5 bit CRC */
  CRCExtracted  = (LC_DataBit[72] & 1) << 4;
  CRCExtracted |= (LC_DataBit[73] & 1) << 3;
  CRCExtracted |= (LC_DataBit[74] & 1) << 2;
  CRCExtracted |= (LC_DataBit[75] & 1) << 1;
  CRCExtracted |= (LC_DataBit[76] & 1) << 0;

  /* Compute the 5 bit CRC */
  CRCComputed = ComputeCrc5Bit(LC_DataBit);

  if(CRCExtracted == CRCComputed) CRCCorrect = 1;
  else CRCCorrect = 0;

  /* Store the Protect Flag (PF) bit */
  TSVoiceSupFrame->FullLC.ProtectFlag = (unsigned int)(LC_DataBit[0]);

  /* Store the Reserved bit */
  TSVoiceSupFrame->FullLC.Reserved = (unsigned int)(LC_DataBit[1]);

  /* Store the Full Link Control Opcode (FLCO)  */
  TSVoiceSupFrame->FullLC.FullLinkControlOpcode = (unsigned int)ConvertBitIntoBytes(&LC_DataBit[2], 6);

  /* Store the Feature set ID (FID)  */
  TSVoiceSupFrame->FullLC.FeatureSetID = (unsigned int)ConvertBitIntoBytes(&LC_DataBit[8], 8);
  //state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;

  /* Store the Service Options  */
  TSVoiceSupFrame->FullLC.ServiceOptions = (unsigned int)ConvertBitIntoBytes(&LC_DataBit[16], 8);

  /* Store the Group address (Talk Group) */
  TSVoiceSupFrame->FullLC.GroupAddress = (unsigned int)ConvertBitIntoBytes(&LC_DataBit[24], 24);

  /* Store the Source address */
  TSVoiceSupFrame->FullLC.SourceAddress = (unsigned int)ConvertBitIntoBytes(&LC_DataBit[48], 24);
  //state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;

  /* Check the CRC values */
  if((IrrecoverableErrors == 0))// && CRCCorrect)
  {
    /* Data is correct */
    //fprintf(stderr, "\nLink Control (LC) Data CRCs are correct !!! Number of error = %u\n", NbOfError);

    /* CRC is correct so consider the Full LC data as correct/valid */
    TSVoiceSupFrame->FullLC.DataValidity = 1;
  }
  else
  {
    /* CRC checking error, so consider the Full LC data as invalid */
    TSVoiceSupFrame->FullLC.DataValidity = 0;
  }

  //ASCII HEX cheat sheet
  // 20 - space; 09 - Horizontal Tab; 0A Line Feed; 0D - Carriage Feed; 00 - Null;
  // 30 - 39 are 0-9 numerals
  // 41 - 5A are upper case A-Z letters
  // 61-7A are lower case a-z letters
  // 7B-7F misc characters (colon, semicolon, etc)
  // let's assume that something has to indicate end of valid characters?

  //Embedded Alias
  if ( TSVoiceSupFrame->FullLC.FullLinkControlOpcode > 0x03 && TSVoiceSupFrame->FullLC.FullLinkControlOpcode < 0x08)
  {
    sprintf (state->dmr_callsign[state->currentslot][TSVoiceSupFrame->FullLC.FullLinkControlOpcode - 3], ""); //blank here so it doesn't grow out of control?
    for (i = 0; i < 10; i++)
    {
      //full range of alphanumerical characters?
      if ( (LC_DataBytes[i] > 0x19 && LC_DataBytes[i] < 0x7F) )
      {
        sprintf ( state->dmr_callsign[state->currentslot][TSVoiceSupFrame->FullLC.FullLinkControlOpcode - 3] +
                  strlen(state->dmr_callsign[state->currentslot][TSVoiceSupFrame->FullLC.FullLinkControlOpcode - 3]) , "%c", LC_DataBytes[i]);
      }
    }
    if (opts->use_ncurses_terminal == 1)
    {
      fprintf (stderr, "%s", KMAG);
      if (state->dmr_stereo == 0)
      {
        fprintf(stderr, "\n");
      }
      fprintf (stderr, "  SLOT %d", state->currentslot+1);
      fprintf (stderr, " Embedded Alias Header and Blocks: [%s%s%s%s%s]", state->dmr_callsign[state->currentslot][0],
              state->dmr_callsign[state->currentslot][1], state->dmr_callsign[state->currentslot][2],
              state->dmr_callsign[state->currentslot][3], state->dmr_callsign[state->currentslot][4] );
      fprintf (stderr, "%s ", KNRM);
    }
  }

  if ( TSVoiceSupFrame->FullLC.FullLinkControlOpcode == 0x08 && opts->payload == 1) //Embedded GPS
  {
    sprintf (state->dmr_callsign[state->currentslot][TSVoiceSupFrame->FullLC.FullLinkControlOpcode - 3], ""); //blank here so it doesn't grow out of control?
    for (i = 0; i < 10; i++)
    {
      //full range of alphanumerical characters?
      if ( (LC_DataBytes[i] > 0x19 && LC_DataBytes[i] < 0x7F) )
      {
        sprintf ( state->dmr_callsign[state->currentslot][TSVoiceSupFrame->FullLC.FullLinkControlOpcode - 3] +
                  strlen(state->dmr_callsign[state->currentslot][TSVoiceSupFrame->FullLC.FullLinkControlOpcode - 3]) , "%c", LC_DataBytes[i]);
      }
    }
    if (opts->use_ncurses_terminal == 1)
    {
      fprintf (stderr, "%s", KCYN);
      if (state->dmr_stereo == 0)
      {
        fprintf(stderr, "\n");
      }
      fprintf (stderr, "  SLOT %d", state->currentslot+1);
      fprintf (stderr, " Embedded GPS: [%s]", state->dmr_callsign[state->currentslot][5] );
      fprintf (stderr, "%s ", KNRM);
    }
  }

  /* Print the destination ID (TG) and the source ID */
  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    if (TSVoiceSupFrame->FullLC.FullLinkControlOpcode < 0x04 || TSVoiceSupFrame->FullLC.FullLinkControlOpcode > 0x08) //other opcodes may convey callsigns, names, etc. was > 0x07
    {
      fprintf (stderr, "%s", KGRN);
      if (state->dmr_stereo == 0)
      {
        fprintf(stderr, "\n");
      }
      fprintf (stderr, "  SLOT %d", state->currentslot+1);
      fprintf(stderr, " TGT=%u  SRC=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
      //fprintf(stderr, "FID=0x%02X ", TSVoiceSupFrame->FullLC.FeatureSetID);
      fprintf (stderr, "%s ", KNRM);
      //fprintf(stderr, "(CRC OK ) ");
      if (state->currentslot == 0)
      {
        state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
        state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
        //state->dmr_color_code = state->color_code;
        state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
        state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
      }
      if (state->currentslot == 1)
      {
        state->lasttgR = TSVoiceSupFrame->FullLC.GroupAddress;
        state->lastsrcR = TSVoiceSupFrame->FullLC.SourceAddress;
        //state->dmr_color_code = state->color_code;
        state->dmr_fidR = TSVoiceSupFrame->FullLC.FeatureSetID;
        state->dmr_soR = TSVoiceSupFrame->FullLC.ServiceOptions;
      }
    }
  }
  else if(IrrecoverableErrors == 0)
  {

    if (TSVoiceSupFrame->FullLC.FullLinkControlOpcode < 0x04 || TSVoiceSupFrame->FullLC.FullLinkControlOpcode > 0x08) //7, or 8?
    {
      fprintf (stderr, "%s", KGRN);
      if (state->dmr_stereo == 0)
      {
        fprintf(stderr, "\n");
      }
      fprintf (stderr, "  SLOT %d", state->currentslot+1);
      fprintf(stderr, " TGT=%u  SRC=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
      //fprintf(stderr, "FID=0x%02X ", TSVoiceSupFrame->FullLC.FeatureSetID);
      fprintf (stderr, "%s ", KRED);
      fprintf(stderr, "RAS (FEC OK/CRC ERR)  ");
      fprintf (stderr, "%s ", KNRM);
      if (state->currentslot == 0)
      {
        state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
        state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
        //state->dmr_color_code = state->color_code;
        state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
        // state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
      }
      if (state->currentslot == 1)
      {
        state->lasttgR = TSVoiceSupFrame->FullLC.GroupAddress;
        state->lastsrcR = TSVoiceSupFrame->FullLC.SourceAddress;
        //state->dmr_color_code = state->color_code;
        state->dmr_fidR = TSVoiceSupFrame->FullLC.FeatureSetID;
        // state->dmr_soR = TSVoiceSupFrame->FullLC.ServiceOptions;
      }
    }
  }
  else {} //fprintf(stderr, "\n(FEC FAIL/CRC ERR)");

#ifdef PRINT_VOICE_BURST_BYTES
  //fprintf(stderr, "\n");
  fprintf(stderr, "VOICE BURST BYTES : ");
  for(i = 0; i < 8; i++)
  {
    fprintf(stderr, "0x%02X", LC_DataBytes[i]);
    if(i != 9) fprintf(stderr, " - ");
  }

  fprintf(stderr, "\n");

  fprintf(stderr, "CRC extracted = 0x%04X - CRC computed = 0x%04X - ", CRCExtracted, CRCComputed);

  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    fprintf(stderr, "CRCs are equal + FEC OK !\n");
  }
  else if(IrrecoverableErrors == 0)
  {
    else fprintf(stderr, "FEC correctly corrected but CRCs are incorrect\n");
  }
  else
  {
    fprintf(stderr, "ERROR !!! CRCs are different and FEC Failed !\n");
  }

  fprintf(stderr, "Hamming Irrecoverable Errors = %u\n", IrrecoverableErrors);
#endif /* PRINT_VOICE_BURST_BYTES */

//Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "%s", KCYN);
    fprintf (stderr, "\nFull Voice Burst Payload ");
    //for (i = 0; i < 8; i++)
    for (i = 0; i < 10; i++)
    {
      fprintf (stderr, "[%02X]", LC_DataBytes[i]); //amateur DMR hams seem to use this area to send callsigns and names using 04,05,06,07 opcodes
    }
    // fprintf(stderr, "\n");
    // fprintf(stderr, "CRC extracted = 0x%04X - CRC computed = 0x%04X - ", CRCExtracted, CRCComputed);
    fprintf (stderr, "%s", KNRM);


  }

} /* End ProcessVoiceBurstSync() */

//LFSR code courtesy of https://github.com/mattames/LFSR/
int LFSR(dsd_state * state)
{
  //int lfsr = 0;
  int lfsr = 0;
  if (state->currentslot == 0)
  {
    lfsr = state->payload_mi;
  }
  else lfsr = state->payload_miR;

  uint8_t cnt = 0;
  //fprintf(stderr, "\nInitial Value:- 0x%08x\n", lfsr);

  for(cnt=0;cnt<32;cnt++)
  {
	  // Polynomial is C(x) = x^32 + x^4 + x^2 + 1
    int bit  = ((lfsr >> 31) ^ (lfsr >> 3) ^ (lfsr >> 1)) & 0x1;
    lfsr =  (lfsr << 1) | (bit);
    //fprintf(stderr, "\nshift #%i, 0x%08x\n", (cnt+1), lfsr);
  }
  if (state->currentslot == 0)
  {
    if (1 == 1)
    {
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, "\n Slot 1");
      fprintf (stderr, " DMR PI Continuation ALG ID: 0x%02X KEY ID: 0x%02X", state->payload_algid, state->payload_keyid);
      fprintf(stderr, " Next MI: 0x%08X", lfsr);
      fprintf (stderr, "%s", KNRM);
    }
    state->payload_mi = lfsr;
  }
  if (state->currentslot == 1) //else
  {
    if (1 == 1)
    {
      fprintf (stderr, "%s", KYEL);
      fprintf (stderr, "\n Slot 2");
      fprintf (stderr, " DMR PI Continuation ALG ID: 0x%02X KEY ID: 0x%02X", state->payload_algidR, state->payload_keyidR);
      fprintf(stderr, " Next MI: 0x%08X", lfsr);
      fprintf (stderr, "%s", KNRM);
    }
    state->payload_miR = lfsr;
  }
}

/*
 * @brief : This function compute the CRC-CCITT of the DMR data
 *          by using the polynomial x^16 + x^12 + x^5 + 1
 *
 * @param Input : A buffer pointer of the DMR data (80 bytes)
 *
 * @return The 16 bit CRC
 */
uint16_t ComputeCrcCCITT(uint8_t * DMRData)
{
  uint32_t i;
  uint16_t CRC = 0x0000; /* Initialization value = 0x0000 */
  /* Polynomial x^16 + x^12 + x^5 + 1
   * Normal     = 0x1021
   * Reciprocal = 0x0811
   * Reversed   = 0x8408
   * Reversed reciprocal = 0x8810 */
  uint16_t Polynome = 0x1021;
  for(i = 0; i < 80; i++)
  {
    if(((CRC >> 15) & 1) ^ (DMRData[i] & 1))
    {
      CRC = (CRC << 1) ^ Polynome;
    }
    else
    {
      CRC <<= 1;
    }
  }

  /* Invert the CRC */
  CRC ^= 0xFFFF;

  /* Return the CRC */
  return CRC;
} /* End ComputeCrcCCITT() */


/*
 * @brief : This function compute the CRC-24 bit of the full
 *          link control by using the Reed-Solomon(12,9) FEC
 *
 * @param FullLinkControlDataBytes : A buffer pointer of the DMR data bytes (12 bytes)
 *
 * @param CRCComputed : A 32 bit pointer where the computed CRC 24-bit will be stored
 *
 * @param CRCMask : The 24 bit CRC mask to apply
 *
 * @return 0 = CRC error
 *         1 = CRC is correct
 */
uint32_t ComputeAndCorrectFullLinkControlCrc(uint8_t * FullLinkControlDataBytes, uint32_t * CRCComputed, uint32_t CRCMask)
{
  uint32_t i;
  rs_12_9_codeword_t VoiceLCHeaderStr;
  rs_12_9_poly_t syndrome;
  uint8_t errors_found = 0;
  rs_12_9_correct_errors_result_t result = RS_12_9_CORRECT_ERRORS_RESULT_NO_ERRORS_FOUND;
  uint32_t CrcIsCorrect = 0;

  for(i = 0; i < 12; i++)
  {
    VoiceLCHeaderStr.data[i] = FullLinkControlDataBytes[i];

    /* Apply CRC mask on each 3 last bytes
     * of the full link control */
    if(i == 9)
    {
      VoiceLCHeaderStr.data[i] ^= (uint8_t)(CRCMask >> 16);
    }
    else if(i == 10)
    {
      VoiceLCHeaderStr.data[i] ^= (uint8_t)(CRCMask >> 8);
    }
    else if(i == 11)
    {
      VoiceLCHeaderStr.data[i] ^= (uint8_t)(CRCMask);
    }
    else
    {
      /* Nothing to do */
    }
  }

  /* Check and correct the full link LC control with Reed Solomon (12,9) FEC */
  rs_12_9_calc_syndrome(&VoiceLCHeaderStr, &syndrome);
  if(rs_12_9_check_syndrome(&syndrome) != 0) result = rs_12_9_correct_errors(&VoiceLCHeaderStr, &syndrome, &errors_found);

  /* Reconstitue the CRC */
  *CRCComputed  = (uint32_t)((VoiceLCHeaderStr.data[9]  << 16) & 0xFF0000);
  *CRCComputed |= (uint32_t)((VoiceLCHeaderStr.data[10] <<  8) & 0x00FF00);
  *CRCComputed |= (uint32_t)((VoiceLCHeaderStr.data[11] <<  0) & 0x0000FF);

  if((result == RS_12_9_CORRECT_ERRORS_RESULT_NO_ERRORS_FOUND) ||
     (result == RS_12_9_CORRECT_ERRORS_RESULT_ERRORS_CORRECTED))
  {
    //fprintf(stderr, "CRC OK : 0x%06X\n", *CRCComputed);
    CrcIsCorrect = 1;

    /* Reconstitue full link control data after FEC correction */
    for(i = 0; i < 12; i++)
    {
      FullLinkControlDataBytes[i] = VoiceLCHeaderStr.data[i];

      /* Apply CRC mask on each 3 last bytes
       * of the full link control */
      if(i == 9)
      {
        FullLinkControlDataBytes[i] ^= (uint8_t)(CRCMask >> 16);
      }
      else if(i == 10)
      {
        FullLinkControlDataBytes[i] ^= (uint8_t)(CRCMask >> 8);
      }
      else if(i == 11)
      {
        FullLinkControlDataBytes[i] ^= (uint8_t)(CRCMask);
      }
      else
      {
        /* Nothing to do */
      }
    }
  }
  else
  {
    //fprintf(stderr, "CRC ERROR : 0x%06X\n", *CRCComputed);
    CrcIsCorrect = 0;
  }

  /* Return the CRC status */
  return CrcIsCorrect;
} /* End ComputeAndCorrectFullLinkControlCrc() */


/*
 * @brief : This function compute the 5 bit CRC of the DMR voice burst data
 *          See ETSI TS 102 361-1 chapter B.3.11
 *
 * @param Input : A buffer pointer of the DMR data (72 bytes)
 *
 * @return The 5 bit CRC
 */
uint8_t ComputeCrc5Bit(uint8_t * DMRData)
{
  uint32_t i, j, k;
  uint8_t  Buffer[9];
  uint32_t Sum;
  uint8_t  CRC = 0;

  /* Convert the 72 bit into 9 bytes */
  k = 0;
  for(i = 0; i < 9; i++)
  {
    Buffer[i] = 0;
    for(j = 0; j < 8; j++)
    {
      Buffer[i] = Buffer[i] << 1;
      Buffer[i] = Buffer[i] | DMRData[k++];
    }
  }

  /* Add all 9 bytes */
  Sum = 0;
  for(i = 0; i < 9; i++)
  {
    Sum += (uint32_t)Buffer[i];
  }

  /* Sum MOD 31 = CRC */
  CRC = (uint8_t)(Sum % 31);

  /* Return the CRC */
  return CRC;
} /* End ComputeCrc5Bit() */

/* End of file */
