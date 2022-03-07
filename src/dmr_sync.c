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

  /* Print the destination ID (TG) and the source ID */
  fprintf(stderr, "| TG=%u  Src=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
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

  if(TSVoiceSupFrame->FullLC.DataValidity) fprintf(stderr, "(OK) ");
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
  fprintf(stderr, "| TG=%u  Src=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
  fprintf(stderr, "FID=0x%02X ", TSVoiceSupFrame->FullLC.FeatureSetID);
  //state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
  //state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
  //state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;


  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    fprintf(stderr, "(OK) ");
    //only set on good CRC value or corrected values
    state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
    state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
    state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
    state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
    state->dmr_color_code = state->color_code;
  }
  else if(IrrecoverableErrors == 0)
  {
    fprintf(stderr, "RAS (FEC OK/CRC ERR)");
    //only set on good CRC value or corrected values
    state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
    state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
    state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
    state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
    state->dmr_color_code = state->color_code;
  }
  else fprintf(stderr, "(FEC FAIL/CRC ERR)");

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
    {
      LC_DataBytes[i] = LC_DataBytes[i] << 1;
      LC_DataBytes[i] = LC_DataBytes[i] | (LC_DataBit[k] & 0x01);
      k++;
      if(k >= 76) break;
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

  /* Print the destination ID (TG) and the source ID */
  fprintf(stderr, "| TG=%u  Src=%u ", TSVoiceSupFrame->FullLC.GroupAddress, TSVoiceSupFrame->FullLC.SourceAddress);
  fprintf(stderr, "FID=0x%02X ", TSVoiceSupFrame->FullLC.FeatureSetID);
  //state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
  //state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
  //state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;

  if((IrrecoverableErrors == 0) && CRCCorrect)
  {
    fprintf(stderr, "(OK)");
    //only set on good CRC?
    state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
    state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
    state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
    state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
    state->dmr_color_code = state->color_code;
  }
  else if(IrrecoverableErrors == 0)
  {
    fprintf(stderr, "RAS (FEC OK/CRC ERR)");
    //or set if was corrected
    state->lasttg = TSVoiceSupFrame->FullLC.GroupAddress;
    state->lastsrc = TSVoiceSupFrame->FullLC.SourceAddress;
    state->dmr_fid = TSVoiceSupFrame->FullLC.FeatureSetID;
    state->dmr_so = TSVoiceSupFrame->FullLC.ServiceOptions;
    state->dmr_color_code = state->color_code;
  }
  else fprintf(stderr, "(FEC FAIL/CRC ERR)");

#ifdef PRINT_VOICE_BURST_BYTES
  fprintf(stderr, "\n");
  fprintf(stderr, "VOICE BURST BYTES : ");
  for(i = 0; i < 10; i++)
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
