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

//#define PRINT_PI_HEADER_BYTES
//#define PRINT_VOICE_LC_HEADER_BYTES
//#define PRINT_TERMINAISON_LC_BYTES
//#define PRINT_VOICE_BURST_BYTES

void Process34Data(dsd_opts * opts, dsd_state * state, unsigned char tdibits[98], uint8_t syncdata[48], uint8_t SlotType[20])
{
  //NEED TRELLIS DECODER HERE
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
  //BPTCDeInterleaveDMRData(info, DeInteleavedData);
  //info is the dibits; is output DeInterleavedData?
  //data is dibits I think, so sub info for data? only wants 98U dibits though.
  //returns 'payload' which is 144 bit array, so DmrDataBit maybe?
  //CDMRTrellisDecode(const unsigned char* data, unsigned char* payload)
  unsigned char tdibits_reverse[98];
  unsigned char tdibits_inverted[98];
  unsigned char tdibits_to_bits[196];
  if (1 == 2)
  {
    fprintf (stderr, "\n Raw Trellis Dibits to Bits\n  ");
    for (i = 0; i < 98; i++)
    {
      tdibits_to_bits[i * 2]       = (tdibits[i] >> 0) & 1;
      tdibits_to_bits[(i * 2) + 1] = (tdibits[1] >> 1) & 1;
      fprintf (stderr, "%d%d", tdibits_to_bits[i * 2], tdibits_to_bits[(i * 2) + 1]);
    }
  }
  if (opts->payload == 1)
  {
    //fprintf (stderr, "\n Raw Trellis Dibits\n  ");
    for (i = 0; i < 98; i++)
    {
      //fprintf (stderr, "#%d [%X] ", i, tdibits[i]);
      //tdibits_reverse[97-i] = tdibits[i];
      //tdibits_reverse[97-i] = ((tdibits[i] & 1)<<1) | ((tdibits[i] & 2)>>1);
      //tdibits_inverted[i] = tdibits[i] ^ 2;
    }
  }

  unsigned char TrellisReturn[18];
  CDMRTrellisDecode(tdibits, TrellisReturn); //figure out how this works!!
  if (opts->payload == 1)
  {
    fprintf (stderr, "\nFull 3/4 Rate Trellis Payload\n  ");
    for (i = 0; i < 18; i++)
    {
      fprintf (stderr, "[%02X]", TrellisReturn[i]);
    }
  }
  /* Extract the BPTC 196,96 DMR data */
  //IrrecoverableErrors = BPTC_196x96_Extract_Data(DeInteleavedData, DmrDataBit, R);
  //
  //CDMRTrellisDibitsToPoints(const signed char* dibits, unsigned char* points) ??
  //
  /* Fill the reserved bit (R(0)-R(2) of the BPTC(196,96) block) */
  //BPTCReservedBits = (R[0] & 0x01) | ((R[1] << 1) & 0x02) | ((R[2] << 2) & 0x08);
  /*
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
  */
  //the reverse card
  for(i = 0, j = 0; i < 18; i++, j+=8)
  {
    DmrDataBit[j + 0] = (TrellisReturn[17-i] >> 0) & 0x01;
    DmrDataBit[j + 1] = (TrellisReturn[17-i] >> 1) & 0x01;
    DmrDataBit[j + 2] = (TrellisReturn[17-i] >> 2) & 0x01;
    DmrDataBit[j + 3] = (TrellisReturn[17-i] >> 3) & 0x01;
    DmrDataBit[j + 4] = (TrellisReturn[17-i] >> 4) & 0x01;
    DmrDataBit[j + 5] = (TrellisReturn[17-i] >> 5) & 0x01;
    DmrDataBit[j + 6] = (TrellisReturn[17-i] >> 6) & 0x01;
    DmrDataBit[j + 7] = (TrellisReturn[17-i] >> 7) & 0x01;
  }
  /* Convert the 96 bit of voice LC Header data into 12 bytes */
  k = 0;
  for(i = 0; i < 18; i++)
  {
    state->dmr_34_rate_sf[i] = state->dmr_34_rate_sf[i+18];    //shift middle to left 12 to load up new round
    state->dmr_34_rate_sf[i+18] = state->dmr_34_rate_sf[i+36]; //shift far right to middle 12, going three frames deep;
    DmrDataByte[i] = 0;
    for(j = 0; j < 8; j++)
    {
      DmrDataByte[i] = DmrDataByte[i] << 1;
      //DmrDataByte[i] = DmrDataByte[i] | (DmrDataBit[k] & 0x01);
      DmrDataByte[i] = DmrDataByte[i] | (DmrDataBit[k] & 0x01);
      k++;
    }
    //state->dmr_34_rate_sf[i+36] = DmrDataByte[i]; //copy byte to right hand side of shift frame
    state->dmr_34_rate_sf[i+36] = TrellisReturn[i];
  }

  /* Fill the CRC extracted (before Reed-Solomon (12,9) FEC correction) */
  CRCExtracted = 0;
  //for(i = 0; i < 24; i++)
  for(i = 0; i < 16; i++)
  {
    //CRCExtracted = CRCExtracted << 1;
    //CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 72] & 1);
    //CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 80] & 1); //80-96 for PI header
  }

  //Look into whether or not we need to run these CRC checks for this header information
  //and see if its applied the same or differently
  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  //CRCExtracted = CRCExtracted ^ 0x969696; //does this mask get applied here though for PI?
  //CRCExtracted = CRCExtracted ^ 0x6969;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  //CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x969696);
  //CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x6969);

  //
  /* Convert corrected 12 bytes into 96 bits */



  //Headers and Addresses
  if ( (state->dmr_34_rate_sf[0] & 0x3F) == 0x05) //0x3F?? or == 0x05
  {
    fprintf (stderr, "\n  IP4 Header");
  }

  if ( (state->dmr_34_rate_sf[0] & 0x3F) == 0x0C)
  {
    fprintf (stderr, "\n  Source:     ");
    fprintf (stderr, " %03d.%03d.%03d.%03d", (state->dmr_34_rate_sf[0] & 0x3F), state->dmr_34_rate_sf[1], state->dmr_34_rate_sf[2], state->dmr_34_rate_sf[3]); //strip first two bits off 1st byte
    fprintf (stderr, " [%08d]", (state->dmr_34_rate_sf[1] <<16 ) + (state->dmr_34_rate_sf[2] << 8) + state->dmr_34_rate_sf[3] );
    fprintf (stderr, " - Port %05d", (state->dmr_34_rate_sf[8] << 8) + state->dmr_34_rate_sf[9]);
    fprintf (stderr, "\n  Destination:");
    fprintf (stderr, " %03d.%03d.%03d.%03d", (state->dmr_34_rate_sf[4] & 0x3F), state->dmr_34_rate_sf[5], state->dmr_34_rate_sf[6], state->dmr_34_rate_sf[7]); //strip first two bits off 4th byte??
    fprintf (stderr, " [%08d]", (state->dmr_34_rate_sf[5] <<16 ) + (state->dmr_34_rate_sf[6] << 8) + state->dmr_34_rate_sf[7] );
    fprintf (stderr, " - Port %05d", (state->dmr_34_rate_sf[10] << 8) + state->dmr_34_rate_sf[11]);
  }
  //LRRP
  if (state->dmr_34_rate_sf[0] == 0x0 && state->dmr_34_rate_sf[4] == 0x0D) //Start LRRP now
  {
    fprintf (stderr, "\n  Data Blocks [%d]", state->dmr_34_rate_sf[5]);
    for (short i = 6; i <= 36; i++)
    {
      if ( state->dmr_34_rate_sf[i] == 0x34 ) //timestamp
      {
        fprintf (stderr, "\n  LRRP - Timestamp: ");
        fprintf (stderr, "%4d.", (state->dmr_34_rate_sf[i+1] << 6) + (state->dmr_34_rate_sf[i+2] >> 2) ); //4 digit year
        fprintf (stderr, "%02d.", ( ((state->dmr_34_rate_sf[i+2] & 0x3) << 2) + ((state->dmr_34_rate_sf[i+3] & 0xC0) >> 6)) ); //2 digit month
        fprintf (stderr, "%02d",  ( (state->dmr_34_rate_sf[i+3] & 0x30) >> 1) + ((state->dmr_34_rate_sf[i+3] & 0x0E) >> 1)  ); //2 digit day
        fprintf (stderr, " %02d:",( (state->dmr_34_rate_sf[i+3] & 0x01) << 4) + ((state->dmr_34_rate_sf[i+4] & 0xF0) >> 4)  ); //2 digit hour
        fprintf (stderr,  "%02d:",( (state->dmr_34_rate_sf[i+4] & 0x0F) << 2) + ((state->dmr_34_rate_sf[i+5] & 0xC0) >> 6)  ); //2 digit minute
        fprintf (stderr,  "%02d", ( (state->dmr_34_rate_sf[i+5] & 0x3F) << 0) );                                               //2 digit second
      }
      if ( state->dmr_34_rate_sf[i] == 0x51 ) //lattitude and longitude
      {
        fprintf (stderr, "\n  LRRP -");
        fprintf (stderr, " Lat: ");
        if (state->dmr_34_rate_sf[i+1] & 0x80) //first bit indicates a sign, or hemisphere?
        {
          fprintf (stderr, "-");
        }
        long int lrrplat;
        long int lrrplon;
        double lat_unit = (double)180/(double)4294967295;
        double lon_unit = (double)360/(double)4294967295;
        lrrplat = ( ( ((state->dmr_34_rate_sf[i+1] & 0x7F ) <<  24 ) + (state->dmr_34_rate_sf[i+2] << 16) + (state->dmr_34_rate_sf[i+3] << 8) + state->dmr_34_rate_sf[i+4]) * 1 );
        lrrplon = ( ( (state->dmr_34_rate_sf[i+5]           <<  24 ) + (state->dmr_34_rate_sf[i+6] << 16) + (state->dmr_34_rate_sf[i+7] << 8) + state->dmr_34_rate_sf[i+8]) * 1 );
        fprintf (stderr, "%.5lf ", ((double)lrrplat) * lat_unit);
        fprintf (stderr, " Lon: ");
        if (state->dmr_34_rate_sf[i+5] & 0x80) //first bit indicates a sign, or hemisphere?
        {
          fprintf (stderr, "-");
        }
        fprintf (stderr, "%.5lf", (lrrplon * lon_unit) );
      }


      if ( state->dmr_34_rate_sf[i] == 0x6C )
      {
        //either Plus is wrong, or I'm wrong on higher velocities exceeding 0xFF.
        //fprintf (stderr, "\n  LRRP - Vi %02X Vf %02X Velocity Units (hex)", state->dmr_34_rate_sf[i+1], state->dmr_34_rate_sf[i+2]);
        double velocity = ( ((double)( (state->dmr_34_rate_sf[i+1] ) + state->dmr_34_rate_sf[i+2] )) / ( (double)128));
        //fprintf (stderr, "\n  LRRP - %.4lf Meters Per Second", velocity);
        fprintf (stderr, "\n  LRRP - %.4lf m/s %.4lf km/h %.4lf mph", velocity, (3.6 * velocity), (2.2369 * velocity) );
      }
      if ( state->dmr_34_rate_sf[i] == 0x56 )
      {
        //check for appropriate terminology here - Heading, bearing, course, or track?
        fprintf (stderr, "\n  LRRP - Direction %d Degrees", state->dmr_34_rate_sf[i+1] * 2);
      }
    }
  }
  //Full
  if (opts->payload == 2)
  {
    fprintf (stderr, "\nFull 3/4 Rate Payload DmrDataByte (reverse bits)\n  ");
    for (i = 0; i < 18; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
  }

}

void ProcessDataData(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{
  //Placeholder
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
    CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 72] & 1);
    //CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 80] & 1); //80-96 for PI header
  }

  //Look into whether or not we need to run these CRC checks for this header information
  //and see if its applied the same or differently
  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  CRCExtracted = CRCExtracted ^ 0x969696; //does this mask get applied here though for PI?
  //CRCExtracted = CRCExtracted ^ 0x6969;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x969696);
  //CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x6969);

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
    fprintf (stderr, "\n(Data CRC Okay)");
  }
  else if((IrrecoverableErrors == 0))
  {
    fprintf (stderr, "\n(Data FEC Okay)");
  }
  else fprintf (stderr, ("\n(Data CRC Fail, FEC Fail)"));

  //
  if (DmrDataByte[0] == 0x43)
  {
    fprintf (stderr, "\n  IP4 Source IP:");
    fprintf (stderr, " %d.%d.%d.%d", (DmrDataByte[0] & 0x3F), DmrDataByte[1], DmrDataByte[2], DmrDataByte[3]); //not sure if these are right or not
    fprintf (stderr, "[%d]", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] << 8) + DmrDataByte[4] );
    fprintf (stderr, " Destination IP:");
    fprintf (stderr, " %d.%d.%d.%d", (DmrDataByte[4] & 0x3F), DmrDataByte[5], DmrDataByte[6], DmrDataByte[7]); //not sure if these are right or not
    fprintf (stderr, "[%d]", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] <<8 ) + DmrDataByte[7] );
  }

  if (DmrDataByte[0] == 0x01)
  {
    fprintf (stderr, "\n  LRRP Control ACK - ");
    fprintf (stderr, " Source:");
    fprintf (stderr, " %d", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] << 8) + DmrDataByte[4] );
    fprintf (stderr, " Destination:");
    fprintf (stderr, " %d", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] <<8 ) + DmrDataByte[7] );
  }
  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "\nFull Data Rate Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
  }

}

void Process1Data(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{
  //Placeholder
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

  //Look into whether or not we need to run these CRC checks for this header information
  //and see if its applied the same or differently
  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  //CRCExtracted = CRCExtracted ^ 0x969696; //does this mask get applied here though for PI?
  //CRCExtracted = CRCExtracted ^ 0x6969;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  //CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x969696);
  //CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x6969);

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
  //
  if (DmrDataByte[0] == 0x43)
  {
    //fprintf (stderr, "\n  IP4 Source IP:");
    //fprintf (stderr, " %d", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] << 8) + DmrDataByte[4] );
    //fprintf (stderr, " Destination IP:");
    //fprintf (stderr, " %d", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] <<8 ) + DmrDataByte[7] );
  }

  if (DmrDataByte[0] == 0x01)
  {
    //fprintf (stderr, "\n  LRRP Control ACK - ");
    //fprintf (stderr, " Source:");
    //fprintf (stderr, " %d", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] << 8) + DmrDataByte[4] );
    //fprintf (stderr, " Destination:");
    //fprintf (stderr, " %d", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] <<8 ) + DmrDataByte[7] );
  }
  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "\nFull Rate 1 Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
  }

}

void ProcessMBChData(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{
  //Placeholder
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

  //Look into whether or not we need to run these CRC checks for this header information
  //and see if its applied the same or differently
  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  //CRCExtracted = CRCExtracted ^ 0x969696; //does this mask get applied here though for PI?
  //CRCExtracted = CRCExtracted ^ 0x6969;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  //CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x969696);
  //CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x6969);

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
  //
  if (DmrDataByte[0] == 0x43)
  {
    //fprintf (stderr, "\n  IP4 Source IP:");
    //fprintf (stderr, " %d", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] << 8) + DmrDataByte[4] );
    //fprintf (stderr, " Destination IP:");
    //fprintf (stderr, " %d", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] <<8 ) + DmrDataByte[7] );
  }

  if (DmrDataByte[0] == 0x01)
  {
    //fprintf (stderr, "\n  LRRP Control ACK - ");
    //fprintf (stderr, " Source:");
    //fprintf (stderr, " %d", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] << 8) + DmrDataByte[4] );
    //fprintf (stderr, " Destination:");
    //fprintf (stderr, " %d", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] <<8 ) + DmrDataByte[7] );
  }
  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "\nFull MBC Header Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
  }

}

void ProcessMBCData(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{
  //Placeholder
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

  //Look into whether or not we need to run these CRC checks for this header information
  //and see if its applied the same or differently
  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  //CRCExtracted = CRCExtracted ^ 0x969696; //does this mask get applied here though for PI?
  //CRCExtracted = CRCExtracted ^ 0x6969;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  //CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x969696);
  //CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x6969);

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
  //
  if (DmrDataByte[0] == 0x43)
  {
    //fprintf (stderr, "\n  IP4 Source IP:");
    //fprintf (stderr, " %d", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] << 8) + DmrDataByte[4] );
    //fprintf (stderr, " Destination IP:");
    //fprintf (stderr, " %d", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] <<8 ) + DmrDataByte[7] );
  }

  if (DmrDataByte[0] == 0x01)
  {
    //fprintf (stderr, "\n  LRRP Control ACK - ");
    //fprintf (stderr, " Source:");
    //fprintf (stderr, " %d", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] << 8) + DmrDataByte[4] );
    //fprintf (stderr, " Destination:");
    //fprintf (stderr, " %d", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] <<8 ) + DmrDataByte[7] );
  }
  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "\nFull MBC Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
  }

}

void ProcessWTFData(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{
  //Placeholder
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

  //Look into whether or not we need to run these CRC checks for this header information
  //and see if its applied the same or differently
  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  //CRCExtracted = CRCExtracted ^ 0x969696; //does this mask get applied here though for PI?
  //CRCExtracted = CRCExtracted ^ 0x6969;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  //CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x969696);
  //CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x6969);

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
  //
  if (DmrDataByte[0] == 0x43)
  {
    //fprintf (stderr, "\n  IP4 Source IP:");
    //fprintf (stderr, " %d", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] << 8) + DmrDataByte[4] );
    //fprintf (stderr, " Destination IP:");
    //fprintf (stderr, " %d", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] <<8 ) + DmrDataByte[7] );
  }

  if (DmrDataByte[0] == 0x01)
  {
    //fprintf (stderr, "\n  LRRP Control ACK - ");
    //fprintf (stderr, " Source:");
    //fprintf (stderr, " %d", (DmrDataByte[2] <<16 ) + (DmrDataByte[3] << 8) + DmrDataByte[4] );
    //fprintf (stderr, " Destination:");
    //fprintf (stderr, " %d", (DmrDataByte[5] <<16 ) + (DmrDataByte[6] <<8 ) + DmrDataByte[7] );
  }
  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "\nFull WTF Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
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
    state->dmr_12_rate_sf[i] = state->dmr_12_rate_sf[i+12];    //shift middle 12 to leftmost 12 to load up new round
    state->dmr_12_rate_sf[i+12] = state->dmr_12_rate_sf[i+24]; //shift far right 12 to middle 12, going three frames deep;
    DmrDataByte[i] = 0;
    for(j = 0; j < 8; j++)
    {
      DmrDataByte[i] = DmrDataByte[i] << 1;
      DmrDataByte[i] = DmrDataByte[i] | (DmrDataBit[k] & 0x01);
      k++;
    }
    state->dmr_12_rate_sf[i+24] = DmrDataByte[i]; //copy byte to right hand side of shift frame
  }

  /* Fill the CRC extracted (before Reed-Solomon (12,9) FEC correction) */
  CRCExtracted = 0;
  //for(i = 0; i < 24; i++)
  for(i = 0; i < 16; i++)
  {
    CRCExtracted = CRCExtracted << 1;
    CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 72] & 1);
    //CRCExtracted = CRCExtracted | (uint32_t)(DmrDataBit[i + 80] & 1); //80-96 for PI header
  }

  //Look into whether or not we need to run these CRC checks for this header information
  //and see if its applied the same or differently
  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  CRCExtracted = CRCExtracted ^ 0x969696; //does this mask get applied here though for PI?
  //CRCExtracted = CRCExtracted ^ 0x6969;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x969696);
  //CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x6969);

  //test
  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    fprintf (stderr, "\n(1/2 Rate CRC Okay)");
  }
  else if((IrrecoverableErrors == 0))
  {
    fprintf (stderr, "\n(1/2 Rate FEC Okay)");
  }
  else fprintf (stderr, ("\n(1/2 Rate CRC Fail, FEC Fail)"));

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

  //Headers and Addresses
  if ( (state->dmr_12_rate_sf[0] & 0x3F) == 0x05) //0x05, or 0x45?
  {
    fprintf (stderr, "\n  IP4 Header");
  }
  //ARS?
  if ( (state->dmr_12_rate_sf[0] & 0x3F) == 0x0C)
  {
    fprintf (stderr, "\n       Source:");
    fprintf (stderr, " %03d.%03d.%03d.%03d", (state->dmr_12_rate_sf[0] & 0x3F), state->dmr_12_rate_sf[1], state->dmr_12_rate_sf[2], state->dmr_12_rate_sf[3]); //strip first two bits off 1st byte
    fprintf (stderr, " [%08d]", (state->dmr_12_rate_sf[1] <<16 ) + (state->dmr_12_rate_sf[2] << 8) + state->dmr_12_rate_sf[3] );
    fprintf (stderr, " - Port %05d", (state->dmr_12_rate_sf[8] << 8) + state->dmr_12_rate_sf[9]);
    fprintf (stderr, "\n  Destination:");
    fprintf (stderr, " %03d.%03d.%03d.%03d", (state->dmr_12_rate_sf[4] & 0x3F), state->dmr_12_rate_sf[5], state->dmr_12_rate_sf[6], state->dmr_12_rate_sf[7]); //strip first two bits off 4th byte??
    fprintf (stderr, " [%08d]", (state->dmr_12_rate_sf[5] <<16 ) + (state->dmr_12_rate_sf[6] << 8) + state->dmr_12_rate_sf[7] );
    fprintf (stderr, " - Port %05d", (state->dmr_12_rate_sf[10] << 8) + state->dmr_12_rate_sf[11]);
  }
  //LRRP
  if (state->dmr_12_rate_sf[0] == 0x0 && state->dmr_12_rate_sf[4] == 0x0D) //Start LRRP now
  {
    //fprintf (stderr, "\n  Data Blocks [%d]", state->dmr_12_rate_sf[5]);
    for (short i = 6; i <= 36; i++)
    {
      if ( state->dmr_12_rate_sf[i] == 0x34 ) //timestamp
      {
        fprintf (stderr, "\n  LRRP - Timestamp: ");
        fprintf (stderr, "%4d.", (state->dmr_12_rate_sf[i+1] << 6) + (state->dmr_12_rate_sf[i+2] >> 2) ); //4 digit year
        fprintf (stderr, "%02d.", ( ((state->dmr_12_rate_sf[i+2] & 0x3) << 2) + ((state->dmr_12_rate_sf[i+3] & 0xC0) >> 6)) ); //2 digit month
        fprintf (stderr, "%02d",  ( (state->dmr_12_rate_sf[i+3] & 0x30) >> 1) + ((state->dmr_12_rate_sf[i+3] & 0x0E) >> 1)  ); //2 digit day
        fprintf (stderr, " %02d:",( (state->dmr_12_rate_sf[i+3] & 0x01) << 4) + ((state->dmr_12_rate_sf[i+4] & 0xF0) >> 4)  ); //2 digit hour
        fprintf (stderr,  "%02d:",( (state->dmr_12_rate_sf[i+4] & 0x0F) << 2) + ((state->dmr_12_rate_sf[i+5] & 0xC0) >> 6)  ); //2 digit minute
        fprintf (stderr,  "%02d", ( (state->dmr_12_rate_sf[i+5] & 0x3F) << 0) );                                               //2 digit second
      }
      if ( state->dmr_12_rate_sf[i] == 0x51 ) //lattitude and longitude
      {
        fprintf (stderr, "\n  LRRP -");
        fprintf (stderr, " Lat: ");
        if (state->dmr_12_rate_sf[i+1] & 0x80) //first bit indicates a sign, or hemisphere?
        {
          fprintf (stderr, "-");
        }
        long int lrrplat;
        long int lrrplon;
        double lat_unit = (double)180/(double)4294967295;
        double lon_unit = (double)360/(double)4294967295;
        lrrplat = ( ( ((state->dmr_12_rate_sf[i+1] & 0x7F ) <<  24 ) + (state->dmr_12_rate_sf[i+2] << 16) + (state->dmr_12_rate_sf[i+3] << 8) + state->dmr_12_rate_sf[i+4]) * 1 );
        lrrplon = ( ( (state->dmr_12_rate_sf[i+5]           <<  24 ) + (state->dmr_12_rate_sf[i+6] << 16) + (state->dmr_12_rate_sf[i+7] << 8) + state->dmr_12_rate_sf[i+8]) * 1 );
        fprintf (stderr, "%.5lf ", ((double)lrrplat) * lat_unit);
        fprintf (stderr, " Lon: ");
        if (state->dmr_12_rate_sf[i+5] & 0x80) //first bit indicates a sign, or hemisphere?
        {
          //fprintf (stderr, "-");
        }
        fprintf (stderr, "%.5lf", (lrrplon * lon_unit) );
        //sprintf ( state->dmr_lrrp[3], "Lat: %.5lf Lon: %.5lf", ((double)lrrplat) * lat_unit , (lrrplon * lon_unit) );
      }


      if ( state->dmr_12_rate_sf[i] == 0x6C )
      {
        //either Plus is wrong, or I'm wrong on higher velocities exceeding 0xFF.
        //fprintf (stderr, "\n  LRRP - Vi %02X Vf %02X Velocity Units (hex)", state->dmr_12_rate_sf[i+1], state->dmr_12_rate_sf[i+2]);
        double velocity = ( ((double)( (state->dmr_12_rate_sf[i+1] ) + state->dmr_12_rate_sf[i+2] )) / ( (double)128));
        //fprintf (stderr, "\n  LRRP - %.4lf Meters Per Second", velocity);
        fprintf (stderr, "\n  LRRP - %.4lf m/s %.4lf km/h %.4lf mph", velocity, (3.6 * velocity), (2.2369 * velocity) );
        //sprintf ( state->dmr_lrrp[1], "Vel: %.4lf kph", (3.6 * velocity));
      }
      if ( state->dmr_12_rate_sf[i] == 0x56 )
      {
        //check for appropriate terminology here - Heading, bearing, course, or track?
        fprintf (stderr, "\n  LRRP - Direction %d Degrees", state->dmr_12_rate_sf[i+1] * 2);
        //sprintf ( state->dmr_lrrp[2], "Dir: %d Deg", state->dmr_12_rate_sf[i+1] * 2);
      }
    }
  }


  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "\nFull 1/2 Rate Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
  }

}

void ProcessCSBK(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{
  //Placeholder
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

  //Look into whether or not we need to run these CRC checks for this header information
  //and see if its applied the same or differently
  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  //CRCExtracted = CRCExtracted ^ 0x969696; //does this mask get applied here though for PI?
  CRCExtracted = CRCExtracted ^ 0x6969;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  //CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x969696);
  CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x6969);

  //test
  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    fprintf (stderr, "\n(CSBK CRC Okay)");
  }
  else if((IrrecoverableErrors == 0))
  {
    fprintf (stderr, "\n(CSBK FEC Okay)");
  }
  else fprintf (stderr, ("\n(CSBK CRC Fail, FEC Fail)"));

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
    fprintf (stderr, "\nMotoTRBO Connect Plus Neighbors\n");
    fprintf(stderr, " NB1(%02x), NB2(%02x), NB3(%02x), NB4(%02x), NB5(%02x)", nb1, nb2, nb3, nb4, nb5);
    sprintf(state->dmr_branding, " MotoTRBO Connect Plus ");
  }
  //ConnectPLUS Section //
  if ( ((csbk_o << 8) +  csbk_fid) == 0x0306 ) //are the FID values attached really necessary
  {
    uint32_t srcAddr = ( (DmrDataByte[2] << 16) + (DmrDataByte[3] << 8) + DmrDataByte[4] ); //extract(csbk, 16, 40);
    uint32_t grpAddr = ( (DmrDataByte[5] << 16) + (DmrDataByte[6] << 8) + DmrDataByte[7] ); //extract(csbk, 40, 64);
    uint8_t  lcn     = ( (DmrDataByte[8] & 0xF0 ) >> 4 ) ; //extract(csbk, 64, 68); 0xF0 LCN not report same as 'plus'?
    uint8_t  tslot   = ( (DmrDataByte[8] & 0x08 ) >> 3 ); //csbk[68]; TS seems fine
    fprintf (stderr, "\nMotoTRBO Connect Plus Channel Grant\n");
    fprintf(stderr, " srcAddr(%8d), grpAddr(%8d), LCN(%d), TS(%d)",srcAddr, grpAddr, lcn, tslot);
    sprintf(state->dmr_branding, " MotoTRBO Connect Plus ");

  }
  if ( ((csbk_o << 8) +  csbk_fid) == 0x0C06 ) //are the FID values attached really necessary?
  {
    uint32_t srcAddr = ( (DmrDataByte[2] << 16) + (DmrDataByte[3] << 8) + DmrDataByte[4] ); //extract(csbk, 16, 40);
    uint32_t grpAddr = ( (DmrDataByte[5] << 16) + (DmrDataByte[6] << 8) + DmrDataByte[7] ); //extract(csbk, 40, 64);
    uint8_t  lcn     = ( (DmrDataByte[8] & 0xF0 ) >> 4 ) ; //extract(csbk, 64, 68); 0xF0 LCN not report same as 'plus'?
    uint8_t  tslot   = ( (DmrDataByte[8] & 0x08 ) >> 3 ); //csbk[68]; TS seems fine
    fprintf (stderr, "\nMotoTRBO Connect Plus Terminate Channel Grant\n"); //Data only shows a srcAddr??
    fprintf(stderr, " srcAddr(%8d), grpAddr(%8d), LCN(%d), TS(%d)",srcAddr, grpAddr, lcn, tslot);
    sprintf(state->dmr_branding, " MotoTRBO Connect Plus ");
  }

  if ( ((csbk_o << 8) +  csbk_fid) == 0x0606 ) //are the FID values attached really necessary?
  {
    uint32_t srcAddr = ( (DmrDataByte[2] << 16) + (DmrDataByte[3] << 8) + DmrDataByte[4] ); //extract(csbk, 16, 40);
    uint32_t grpAddr = ( (DmrDataByte[5] << 16) + (DmrDataByte[6] << 8) + DmrDataByte[7] ); //extract(csbk, 40, 64);
    uint8_t  lcn     = ( (DmrDataByte[8] & 0xF0 ) >> 4 ) ; //extract(csbk, 64, 68); 0xF0 LCN not report same as 'plus'?
    uint8_t  tslot   = ( (DmrDataByte[8] & 0x08 ) >> 3 ); //csbk[68]; TS seems fine
    fprintf (stderr, "\nMotoTRBO Connect Plus Data Channel Grant\n"); //Data I have doesn't show any values for lcn or tslot
    fprintf(stderr, " srcAddr(%8d), grpAddr(%8d), LCN(%d), TS(%d)",srcAddr, grpAddr, lcn, tslot);
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
    fprintf (stderr, "\n  CSBK Aloha?");
    //fprintf (stderr, "\n  Site ID %d-%d.%d", (DmrDataByte[2] & 0xF0 >> 4), DmrDataByte[2] & 0xF, DmrDataByte[3]);
    //fprintf (stderr, "  DCC %d", DmrDataByte[1]);
    //sprintf ( state->dmr_callsign[0], "Site ID %d-%d.%d", (DmrDataByte[2] & 0xF0 >> 4), DmrDataByte[2] & 0xF, DmrDataByte[3]);
    //sprintf(state->dmr_branding, " TIII "); //?? one of these next two seems to be on both types, maybe its a TIII thing?
  }


  //Hytera XPT
  if (csbk_o == 0x28)
  {
    fprintf (stderr, "\nHytera TIII Announcement");
    //sprintf(state->dmr_branding, " MotoTRBO Capacity Plus ");
    sprintf(state->dmr_branding, " TIII "); //?? one of these next two seems to be on both types, maybe its a TIII thing?
  }
  //if ( ((csbk_o << 8) +  csbk_fid) == 0x3606 ) //
  if (csbk_o == 0x36)
  {
    fprintf (stderr, "\nHytera XPT");
    sprintf(state->dmr_branding, " Hytera XPT ");
  }

  if (csbk_o == 0x0A)
  {
    fprintf (stderr, "\nHytera XPT Site State");
    sprintf(state->dmr_branding, " Hytera XPT ");
  }

  //if ( ((csbk_o << 8) +  csbk_fid) == 0x3706 ) //
  if (csbk_o == 0x37)
  {
    fprintf (stderr, "\nHytera XPT");
    sprintf(state->dmr_branding, " Hytera XPT ");
  }

  //Full
  if (opts->payload == 1)
  {
    fprintf (stderr, "\nFull CSBK Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
  }

}

void ProcessDmrPIHeader(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t syncdata[48], uint8_t SlotType[20])
{
  //Placeholder
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

  /* Remove warning compiler */
  //UNUSED_VARIABLE(syncdata[0]);
  //UNUSED_VARIABLE(SlotType[0]);
  //UNUSED_VARIABLE(BPTCReservedBits);

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

  //Look into whether or not we need to run these CRC checks for this header information
  //and see if its applied the same or differently
  /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
  //CRCExtracted = CRCExtracted ^ 0x969696; //does this mask get applied here though for PI?
  CRCExtracted = CRCExtracted ^ 0x6969;

  /* Check/correct the full link control data and compute the Reed-Solomon (12,9) CRC */
  //CRCCorrect = ComputeAndCorrectFullLinkControlCrc(DmrDataByte, &CRCComputed, 0x969696);
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


  state->payload_algid = DmrDataByte[0];
  state->payload_keyid = DmrDataByte[2];
  state->payload_mi    = ( ((DmrDataByte[3]) << 24) + ((DmrDataByte[4]) << 16) + ((DmrDataByte[5]) << 8) + (DmrDataByte[6]) );
  if (1 == 1) //have it always print?
  {
    fprintf (stderr, "\n DMR PI Header ALG ID: 0x%02X KEY ID: 0x%02X MI: 0x%08X", state->payload_algid, state->payload_keyid, state->payload_mi);
  }

  //test
  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    fprintf (stderr, " (PI CRC Okay)");
  }
  else if((IrrecoverableErrors == 0))
  {
    fprintf (stderr, " (PI FEC Okay)");
  }
  else fprintf (stderr, (" (PI CRC Fail, FEC Fail)"));

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

  /* Remove warning compiler */
  //UNUSED_VARIABLE(syncdata[0]);
  //UNUSED_VARIABLE(SlotType[0]);
  //UNUSED_VARIABLE(BPTCReservedBits);

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
  //state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
  /* Store the Source address */
  TSVoiceSupFrame->FullLC.SourceAddress = (unsigned int)ConvertBitIntoBytes(&DmrDataBit[48], 24);
  //state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;

  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    /* CRC is correct so consider the Full LC data as correct/valid */
    TSVoiceSupFrame->FullLC.DataValidity = 1;
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
  fprintf(stderr, "\n  TG=%u  Src=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
  fprintf(stderr, "FID=0x%02X ", TSVoiceSupFrame->FullLC.FeatureSetID);
  //state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
  //state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
  //state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;

  //HERE HERE do some work to get this Emergency and BP modes to display in ncurses
  //state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;

  if(TSVoiceSupFrame->FullLC.ServiceOptions & 0x80) fprintf(stderr, "Emergency ");
  if(TSVoiceSupFrame->FullLC.ServiceOptions & 0x40)
  {
    /* By default select the basic privacy (BP), if the encryption mode is EP ARC4 or AES256
     * a PI Header will be sent with the encryption mode and DSD will upgrade automatically
     * the new encryption mode */
    opts->EncryptionMode = MODE_BASIC_PRIVACY;

    fprintf(stderr, "Encrypted ");
  }
  else
  {
    opts->EncryptionMode = MODE_UNENCRYPTED;
    fprintf(stderr, "Clear/Unencrypted ");
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

  if(TSVoiceSupFrame->FullLC.DataValidity) fprintf(stderr, "(CRC OK )  ");
  else if(IrrecoverableErrors == 0) fprintf(stderr, "RAS (FEC OK/CRC ERR)");
  else fprintf(stderr, "(FEC FAIL/CRC ERR)");

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
    fprintf (stderr, "\nFull Voice Header Payload ");
    for (i = 0; i < 12; i++)
    {
      fprintf (stderr, "[%02X]", DmrDataByte[i]);
    }
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

  /* Remove warning compiler */
  //UNUSED_VARIABLE(opts);
  //UNUSED_VARIABLE(syncdata[0]);
  //UNUSED_VARIABLE(SlotType[0]);
  //UNUSED_VARIABLE(BPTCReservedBits);

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
  //state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
  /* Store the Source address */
  TSVoiceSupFrame->FullLC.SourceAddress = (unsigned int)ConvertBitIntoBytes(&DmrDataBit[48], 24);
  //state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
  //TSVoiceSupFrame->FullLC.LeftOvers = (unsigned int)ConvertBitIntoBytes(&DmrDataBit[64], 8);
  //fprintf (stderr, "\nBPTC Left Overs = %02X \n", TSVoiceSupFrame->FullLC.LeftOvers);
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

  /* Print the destination ID (TG) and the source ID */
  //fprintf(stderr, "\n  TG=%u  Src=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
  //fprintf(stderr, "FID=0x%02X ", TSVoiceSupFrame->FullLC.FeatureSetID);

  //reset alg, keys, mi during a TLC call termination EVENT so we aren't stuck on an old value, PI header will proceed a new call if BP isn't used
  state->payload_algid = 0;
  state->payload_keyid = 0;
  //state->payload_mfid  = 0;
  state->payload_mi    = 0;

  //tlc
  if((IrrecoverableErrors == 0) && CRCCorrect) //amateur DMR seems to only set radio ID up here I think, figure out best way to set without messing up other DMR types
  {
    fprintf(stderr, "\n  TG=%u  Src=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
    fprintf(stderr, "FID=0x%02X ", TSVoiceSupFrame->FullLC.FeatureSetID);
    fprintf(stderr, "(CRC OK )  ");
    if (TSVoiceSupFrame->FullLC.FullLinkControlOpcode == 0) //other opcodes may convey callsigns, names, etc.
    /*
    if ( TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0xAF   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x04   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x05   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x06   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x07   ) //work out why amateur sets sourceid on other codes, but pro only does on whatever
    */
    {
      state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
      state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
      state->dmr_color_code = state->color_code;
      state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
      state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
    }
    //only set on good CRC value or corrected values
    //state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
    //state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
    //state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
    //state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
    //state->dmr_color_code = state->color_code;
  }
  else if(IrrecoverableErrors == 0)
  {
    fprintf(stderr, "\n  TG=%u  Src=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
    fprintf(stderr, "FID=0x%02X ", TSVoiceSupFrame->FullLC.FeatureSetID);
    fprintf(stderr, "RAS (FEC OK/CRC ERR)"); //tlc
    if (TSVoiceSupFrame->FullLC.FullLinkControlOpcode == 0) //other opcodes may convey callsigns, names, etc.
    /*
    if ( TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0xAF   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x04   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x05   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x06   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x07   ) //work out why amateur sets sourceid on other codes, but pro only does on whatever
    */
    {
      state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
      state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
      state->dmr_color_code = state->color_code;
      state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
      state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
    }
    //only set on good CRC value or corrected values
    //state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
    //state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
    //state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
    //state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
    //state->dmr_color_code = state->color_code;
  }
  else fprintf(stderr, "\n(FEC FAIL/CRC ERR)");

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
  //fprintf (stderr, "\nLCDB = 0x%X \n", LC_DataBit);
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
  //state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
  /* Store the Source address */
  TSVoiceSupFrame->FullLC.SourceAddress = (unsigned int)ConvertBitIntoBytes(&LC_DataBit[48], 24);
  //state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;

  /* Check the CRC values */
  if((IrrecoverableErrors == 0))// && CRCCorrect)
  {
    /* Data ara correct */
    //fprintf(stderr, "\nLink Control (LC) Data CRCs are correct !!! Number of error = %u\n", NbOfError);

    /* CRC is correct so consider the Full LC data as correct/valid */
    TSVoiceSupFrame->FullLC.DataValidity = 1;
  }
  else
  {
    /* CRC checking error, so consider the Full LC data as invalid */
    TSVoiceSupFrame->FullLC.DataValidity = 0;
  }

  //GPS, not sure if in right place or not though
  if (TSVoiceSupFrame->FullLC.FullLinkControlOpcode == 0x08)
  {
    long int lrrplat;
    long int lrrplon;
    double lat_unit = (double)180/(double)4294967295; //damned floats screweing me over lol
    double lon_unit = (double)360/(double)4294967295;
    //lrrplat = ( ( ((DmrDataByte[0] & 0x3F) <<  24 ) + (DmrDataByte[1] << 16) + (DmrDataByte[2] << 8) + DmrDataByte[3]) * 1 ); //(0x0B4/0xFFFFFFFF)
    //lrrplon = ( ( (DmrDataByte[4] <<  24 )          + (DmrDataByte[5] << 16) + (DmrDataByte[6] << 8) + DmrDataByte[7]) * 1 ); //(0x168/0xFFFFFFFF) //0x3F, or whole thing?;

    //fprintf (stderr, "  Standard GPS Coordinates?? Check Payload.\n");
    //fprintf (stderr, "Figure out what to put here");

  }

  //ASCII HEX cheat sheet
  // 20 - space; 09 - Horizontal Tab; 0A Line Feed; 0D - Carriage Feed; 00 - Null;
  // 30 - 39 are 0-9 numerals
  // 41 - 5A are upper case A-Z letters
  // 61-7A are lower case a-z letters
  //
  // let's assume that something has to indicate end of valid characters?
  if ( TSVoiceSupFrame->FullLC.FullLinkControlOpcode > 0x03 &&  TSVoiceSupFrame->FullLC.FullLinkControlOpcode < 0x08 && opts->payload == 1)
  {
    //fprintf (stderr, "Call Sign %X [ ", TSVoiceSupFrame->FullLC.FullLinkControlOpcode - 3);
    sprintf (state->dmr_callsign[TSVoiceSupFrame->FullLC.FullLinkControlOpcode - 3], ""); //blank here so it doesn't grow out of control?
    for (i = 0; i < 10; i++)
    {
      if (
         //(LC_DataBytes[i] > 0x29 && LC_DataBytes[i] < 0x3A) || //numerals
         //(LC_DataBytes[i] > 0x40 && LC_DataBytes[i] < 0x5B) || //uppers
         //(LC_DataBytes[i] > 0x60 && LC_DataBytes[i] < 0x7B) || //lowers
         //(LC_DataBytes[i] == 0x20) )                           //space
         (LC_DataBytes[i] > 0x19 && LC_DataBytes[i] < 0x7F) )    //full range of 'normal' characters?
      {
        //fprintf (stderr, "%c", LC_DataBytes[i]);
        sprintf ( state->dmr_callsign[TSVoiceSupFrame->FullLC.FullLinkControlOpcode - 3] +
                  strlen(state->dmr_callsign[TSVoiceSupFrame->FullLC.FullLinkControlOpcode - 3]) , "%c", LC_DataBytes[i]);
      }
    }
    //fprintf (stderr, "]\n");
    if (opts->use_ncurses_terminal == 1)
    {
      fprintf (stderr, "\n  Alias Header and Blocks: [%s%s%s%s%s]", state->dmr_callsign[0], state->dmr_callsign[1], state->dmr_callsign[2], state->dmr_callsign[3], state->dmr_callsign[4] );
    }
  }

  /* Print the destination ID (TG) and the source ID */
  if (TSVoiceSupFrame->FullLC.FullLinkControlOpcode < 0x04 || TSVoiceSupFrame->FullLC.FullLinkControlOpcode > 0x09) //voice burst, was > 0x07
  {
    //fprintf(stderr, "  TG=%u  Src=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
    //fprintf(stderr, "FID=0x%02X ", TSVoiceSupFrame->FullLC.FeatureSetID);
    //state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
    //state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
  }

  //state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
  //state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
  //state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;

  if((IrrecoverableErrors == 0) && CRCCorrect) //voice burst
  {
    //fprintf(stderr, "  TG=%u  Src=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
    //fprintf(stderr, "FID=0x%02X ", TSVoiceSupFrame->FullLC.FeatureSetID);
    //fprintf(stderr, "(CRC OK )  ");
    //only set on good CRC?
    if (TSVoiceSupFrame->FullLC.FullLinkControlOpcode < 0x04 || TSVoiceSupFrame->FullLC.FullLinkControlOpcode > 0x09) //other opcodes may convey callsigns, names, etc. was > 0x07
    /*
    if ( TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0xAF   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x04   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x05   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x06   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x07   ) //work out why amateur sets sourceid on other codes, but pro only does on whatever
    */
    {
      fprintf(stderr, "\n  TG=%u  Src=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
      fprintf(stderr, "FID=0x%02X ", TSVoiceSupFrame->FullLC.FeatureSetID);
      fprintf(stderr, "(CRC OK ) ");
      state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
      state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
      state->dmr_color_code = state->color_code;
      //state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
      //state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
    }
    //state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
    //state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
    state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
    state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
    //state->dmr_color_code = state->color_code;
  }
  else if(IrrecoverableErrors == 0)
  {
    //fprintf(stderr, "  TG=%u  Src=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
    //fprintf(stderr, "FID=0x%02X ", TSVoiceSupFrame->FullLC.FeatureSetID);
    //fprintf(stderr, "RAS (FEC OK/CRC ERR)"); //voice burst
    //or set if was corrected
    if (TSVoiceSupFrame->FullLC.FullLinkControlOpcode < 0x04 || TSVoiceSupFrame->FullLC.FullLinkControlOpcode > 0x07) //other opcodes may convey callsigns, names, etc.
    /*
    if ( TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0xAF   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x04   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x05   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x06   ||
         TSVoiceSupFrame->FullLC.FullLinkControlOpcode != 0x07   ) //other opcodes may convey callsigns, names, etc.
    */
    {
      fprintf(stderr, "\n  TG=%u  Src=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
      fprintf(stderr, "FID=0x%02X ", TSVoiceSupFrame->FullLC.FeatureSetID);
      fprintf(stderr, "RAS (FEC OK/CRC ERR)  ");
      state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
      state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
      state->dmr_color_code = state->color_code;
      //state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
      //state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
    }
    //state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
    //state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
    state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
    state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
    //state->dmr_color_code = state->color_code;
  }
  else fprintf(stderr, "\n(FEC FAIL/CRC ERR)");

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
    fprintf (stderr, "\nFull Voice Burst Payload ");
    //for (i = 0; i < 8; i++)
    for (i = 0; i < 10; i++)
    {
      fprintf (stderr, "[%02X]", LC_DataBytes[i]); //amateur DMR hams seem to use this area to send callsigns and names using 04,05,06,07 opcodes
    }
    //fprintf (stderr, "\n");
  }

} /* End ProcessVoiceBurstSync() */


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


/*
 * @brief : This function returns the Algorithm ID into an explicit string
 *
 * @param AlgID : The algorithm ID
 *   @arg : 0x21 for ARC4
 *   @arg : 0x22 for DES
 *   @arg : 0x25 for AES256
 *
 * @return A constant string pointer that explain the Alg ID used
 */
uint8_t * DmrAlgIdToStr(uint8_t AlgID)
{
  if(AlgID == 0x21) return (uint8_t *)"ARC4";
  else if(AlgID == 0x25) return (uint8_t *)"AES256";
  else return (uint8_t *)"UNKNOWN";
  //state->payload_algid = AlgID;
} /* End DmrAlgIdToStr */

/*
 * @brief : This function returns the encryption mode into an explicit string
 *
 * @param PrivacyMode : The algorithm ID
 *   @arg : MODE_UNENCRYPTED
 *   @arg : MODE_BASIC_PRIVACY
 *   @arg : MODE_ENHANCED_PRIVACY_ARC4
 *   @arg : MODE_ENHANCED_PRIVACY_DES
 *   @arg : MODE_ENHANCED_PRIVACY_AES256
 *   @arg : MODE_HYTERA_BASIC_40_BIT
 *   @arg : MODE_HYTERA_BASIC_128_BIT
 *   @arg : MODE_HYTERA_BASIC_256_BIT
 *
 * @return A constant string pointer that explain the encryption mode used
 */
uint8_t * DmrAlgPrivacyModeToStr(uint32_t PrivacyMode)
{
  switch(PrivacyMode)
  {
    case MODE_UNENCRYPTED:
    {
      return (uint8_t *)"NOT ENC";
      break;
    }
    case MODE_BASIC_PRIVACY:
    {
      return (uint8_t *)"BP";
      break;
    }
    case MODE_ENHANCED_PRIVACY_ARC4:
    {
      return (uint8_t *)"EP ARC4";
      break;
    }
    case MODE_ENHANCED_PRIVACY_AES256:
    {
      return (uint8_t *)"EP AES256";
      break;
    }
    case MODE_HYTERA_BASIC_40_BIT:
    {
      return (uint8_t *)"HYTERA BASIC 40 BIT";
      break;
    }
    case MODE_HYTERA_BASIC_128_BIT:
    {
      return (uint8_t *)"HYTERA BASIC 128 BIT";
      break;
    }
    case MODE_HYTERA_BASIC_256_BIT:
    {
      return (uint8_t *)"HYTERA BASIC 256 BIT";
      break;
    }
    default:
    {
      return (uint8_t *)"UNKNOWN";
      break;
    }
  } /* End switch(PrivacyMode) */
} /* End DmrAlgPrivacyModeToStr() */


/* End of file */
