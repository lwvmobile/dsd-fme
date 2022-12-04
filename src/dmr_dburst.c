/*-------------------------------------------------------------------------------
 * dmr_dburst.c
 * DMR Data Burst Handling and related BPTC/FEC/CRC Functions
 *
 * Portions of BPTC/FEC/CRC code from LouisErigHerve
 * Source: https://github.com/LouisErigHerve/dsd/blob/master/src/dmr_sync.c
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

//TODO: CRC9 on confirmed data blocks; CRC32 on completed data sf;
//TODO: Embedded GPS Calculations
//TODO: MBC full message CRC16 (does this include the header or not though?)
//TODO: Reverse Channel Signalling - RC single burst BPTC/7-bit CRC
//TODO: Single Burst Embedded LC - Non-RC single burst LC - Look for Late Entry Alg/Key

#include "dsd.h"

void dmr_data_burst_handler(dsd_opts * opts, dsd_state * state, uint8_t info[196], uint8_t databurst) 
{

  uint32_t i, j, k;
  uint32_t CRCExtracted     = 0;
  uint32_t CRCComputed      = 0;
  uint32_t CRCCorrect       = 0;
  uint32_t IrrecoverableErrors = 0;
  uint8_t slot = state->currentslot;
  uint8_t block = state->data_block_counter[slot];

  //BPTC 196x96 Specific
  uint8_t  BPTCDeInteleavedData[196];
  uint8_t  BPTCDmrDataBit[96];
  uint8_t  BPTCDmrDataByte[12];

  //Embedded Signalling Specific
  uint8_t  BptcDataMatrix[8][16];
  uint8_t  LC_DataBit[77];
  uint8_t  LC_DataBytes[10];
  int Burst = -1;

  //Unified PDU (is this the best nomenclature) Bytes
  uint8_t  DMR_PDU[25]; //using 25 bytes as the max for now
  uint8_t  DMR_PDU_bits[196]; //bitwise version of unified payload, will probably be a lot easier to use this w/ manual
  memset (DMR_PDU, 0, sizeof (DMR_PDU));
  memset (DMR_PDU_bits, 0, sizeof (DMR_PDU_bits));

  uint8_t  R[3];
  uint8_t  BPTCReservedBits = 0;

  uint32_t crcmask = 0; 
  uint8_t crclen = 0;

  uint8_t is_bptc = 0;
  uint8_t is_trellis = 0; 
  uint8_t is_emb = 0;
  uint8_t is_lc = 0;  
  uint8_t is_full = 0; 
  uint8_t pdu_len = 0;
  uint8_t pdu_start = 0; //starting value of pdu (0 normal, 2 for confirmed) 

  switch(databurst){
    case 0x00: //PI
      is_bptc = 1;
      crclen = 16;
      crcmask = 0x6969; //insert pi and 69 jokes here
      pdu_len = 12; //12 bytes
      sprintf(state->fsubtype, " PI  ");
      break;
    case 0x01: //VLC
      is_bptc = 1;
      is_lc = 1;
      crclen = 24;
      crcmask = 0x969696; 
      pdu_len = 12; //12 bytes
      sprintf(state->fsubtype, " VLC ");
      break;
    case 0x02: //TLC
      is_bptc = 1;
      is_lc = 1;
      crcmask = 0x999999; 
      crclen = 24;
      pdu_len = 12; //12 bytes
      sprintf(state->fsubtype, " TLC ");
      break;
    case 0x03: //CSBK
      is_bptc = 1;
      crclen = 16;
      crcmask = 0xA5A5; 
      pdu_len = 12; //12 bytes
      sprintf(state->fsubtype, " CSBK");
      break;
    case 0x04: //MBC Header
      is_bptc = 1;
      crclen = 16;
      crcmask = 0xAAAA;
      pdu_len = 12; //12 bytes
      sprintf(state->fsubtype, " MBCH");
      break;
    case 0x05: //MBC Continuation
      is_bptc = 1;
      //let block assembler carry out crc on the completed message
      pdu_len = 12; //12 bytes
      sprintf(state->fsubtype, " MBCC");
      break;
    case 0x06: //Data Header
      is_bptc = 1;
      crclen = 16;
      crcmask = 0xCCCC;
      pdu_len = 12; //12 bytes
      sprintf(state->fsubtype, " DATA");
      break;
    //TODO - 1/2 0x0F0 - 3/4 0x1FF - Rate 1 0x10F 
    //going to set crclen and mask according to confirmed data here,
    //use block assmebler to carry out completed message crc
    case 0x07: //1/2 Rate Data
      is_bptc = 1;
      crclen = 9; //confirmed data only
      crcmask = 0x0F0; 
      pdu_len = 12; //12 bytes unconfirmed
      if (state->data_conf_data[slot] == 1)
      {
        pdu_len = 10; 
        pdu_start = 2; //start at plus two when assembling
      } 
      sprintf(state->fsubtype, " R12D ");
      break;
    case 0x08: //3/4 Rate Data
      is_trellis = 1;
      crclen = 9; //confirmed data only
      crcmask = 0x1FF;
      pdu_len = 18; //18 bytes unconfirmed
      if (state->data_conf_data[slot] == 1)
      {
        pdu_len = 16; 
        pdu_start = 2; //start at plus two when assembling
      } 
      sprintf(state->fsubtype, " R34D ");
      break;
    case 0x09: //Idle
      //pseudo-random data fill, no need to do anything with this
      sprintf(state->fsubtype, " IDLE ");
      break;
    case 0x0A: //1 Rate Data
      crclen = 9; //confirmed data only
      crcmask = 0x1FF; 
      is_full = 1;
      pdu_len = 25; //196 bits - 24.5 bytes 
      sprintf(state->fsubtype, " R_1D ");
      break;
    case 0x0B: //Unified Data
      is_bptc = 1;
      crclen = 16;
      crcmask = 0x3333;
      pdu_len = 12; //12 bytes
      sprintf(state->fsubtype, " UDAT ");
      break;
    //special types (not real data 'sync' bursts)
    case 0xEB: //Embedded Signalling
      is_emb = 1;
      pdu_len = 9;
      break;

    default:
      break;
    
  }

  if (databurst != 0xEB)
  {
    if (state->dmr_ms_mode == 0) fprintf(stderr, "| Color Code=%02d ", state->dmr_color_code);
    fprintf(stderr, "|%s", state->fsubtype);
  }
  
  //Most Data Sync Burst types will use the bptc 196x96
  if (is_bptc)
  {
    CRCExtracted = 0;
    CRCComputed = 0;
    IrrecoverableErrors = 0;

    /* Deinterleave DMR data */
    BPTCDeInterleaveDMRData(info, BPTCDeInteleavedData);

    /* Extract the BPTC 196,96 DMR data */
    IrrecoverableErrors = BPTC_196x96_Extract_Data(BPTCDeInteleavedData, BPTCDmrDataBit, R);

    /* Fill the reserved bit (R(0)-R(2) of the BPTC(196,96) block) */
    BPTCReservedBits = (R[0] & 0x01) | ((R[1] << 1) & 0x02) | ((R[2] << 2) & 0x08);

    /* Convert the 96 bits BPTC data into 12 bytes */
    k = 0;
    for(i = 0; i < 12; i++)
    {
      BPTCDmrDataByte[i] = 0;
      for(j = 0; j < 8; j++)
      {
        BPTCDmrDataByte[i] = BPTCDmrDataByte[i] << 1;
        BPTCDmrDataByte[i] = BPTCDmrDataByte[i] | (BPTCDmrDataBit[k] & 0x01);
        k++;
      }
    }

    /* Fill the CRC extracted (before Reed-Solomon (12,9) FEC correction) */
    CRCExtracted = 0;
    for(i = 0; i < crclen; i++) 
    {
      CRCExtracted = CRCExtracted << 1;
      CRCExtracted = CRCExtracted | (uint32_t)(BPTCDmrDataBit[i + 96 - crclen] & 1); 
    }

    /* Apply the CRC mask (see DMR standard B.3.12 Data Type CRC Mask) */
    CRCExtracted = CRCExtracted ^ crcmask;

    /* Check/correct the BPTC data and compute the Reed-Solomon (12,9) CRC */ 
    if (is_lc) CRCCorrect = ComputeAndCorrectFullLinkControlCrc(BPTCDmrDataByte, &CRCComputed, crcmask);

    //TODO: run CRC9 on confirmed 1/2 data blocks
    else if (state->data_conf_data[slot] == 1)
    {
      //run CRC on individual blocks according to type and validate each individual block
    }

    //run CCITT on other data forms
    else //will still run on unconfirmed data (shouldn't matter other than a possible bogus warning message)
    {
      CRCComputed = ComputeCrcCCITT(BPTCDmrDataBit); 
      if (CRCComputed == CRCExtracted) CRCCorrect = 1;
      else CRCCorrect = 0;
    } 

    if (databurst == 0x04 || databurst == 0x06) //MBC Header, Data Header
    {
      if (CRCCorrect)
      {
        state->data_block_crc_valid[slot][0] = 1;
      }
      else state->data_block_crc_valid[slot][0] = 0;
    }
    
    /* Convert corrected x bytes into x*8 bits */
    for(i = 0, j = 0; i < pdu_len; i++, j+=8)
    {
      BPTCDmrDataBit[j + 0] = (BPTCDmrDataByte[i+pdu_start] >> 7) & 0x01;
      BPTCDmrDataBit[j + 1] = (BPTCDmrDataByte[i+pdu_start] >> 6) & 0x01;
      BPTCDmrDataBit[j + 2] = (BPTCDmrDataByte[i+pdu_start] >> 5) & 0x01;
      BPTCDmrDataBit[j + 3] = (BPTCDmrDataByte[i+pdu_start] >> 4) & 0x01;
      BPTCDmrDataBit[j + 4] = (BPTCDmrDataByte[i+pdu_start] >> 3) & 0x01;
      BPTCDmrDataBit[j + 5] = (BPTCDmrDataByte[i+pdu_start] >> 2) & 0x01;
      BPTCDmrDataBit[j + 6] = (BPTCDmrDataByte[i+pdu_start] >> 1) & 0x01;
      BPTCDmrDataBit[j + 7] = (BPTCDmrDataByte[i+pdu_start] >> 0) & 0x01;
    }

    //convert to unified DMR_PDU and DMR_PDU_bits
    for (i = 0; i < pdu_len; i++)
    {
      DMR_PDU[i] = BPTCDmrDataByte[i+pdu_start];
    }

    for (i = 0; i < pdu_len * 8; i++)
    {
      DMR_PDU_bits[i] = BPTCDmrDataBit[i];
    }

  }

  //Embedded Signalling will use BPTC 128x77
  if (is_emb)
  {
    
    CRCExtracted = 0;
    CRCComputed = 0;
    IrrecoverableErrors = 0;

    /* First step : Reconstitute the BPTC 16x8 matrix */
    Burst = 1; /* Burst B to E contains embedded signaling data */
    k = 0;
    for(i = 0; i < 16; i++)
    {
      for(j = 0; j < 8; j++)
      {
        /* Only the LSBit of the byte is stored */
        BptcDataMatrix[j][i] = state->dmr_embedded_signalling[slot][Burst][k+8];
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
      
    for (i = 0; i < 72; i++)
    {
      DMR_PDU_bits[i] = LC_DataBit[i];
    }

    for (i = 0; i < 9; i++)
    {
      DMR_PDU[i] = (uint8_t)ConvertBitIntoBytes(&LC_DataBit[i*8], 8);
    }

  }

  //the ugly one
  if (is_trellis)
  {
    CRCExtracted = 0;
    CRCComputed = 0;
    IrrecoverableErrors = 1; //can't be tested, but maybe can poll trellis decoder for a value in the future

    uint8_t tdibits[98];
    memset (tdibits, 0, sizeof(tdibits));

    //reconstitute info bits into reverse ordered dibits for the trellis decoder
    for (i = 0; i < 98; i++)
    {
      tdibits[97-i] = (info[i*2] << 1) | info[i*2+1]; 
    }

    unsigned char TrellisReturn[18];
    memset (TrellisReturn, 0, sizeof(TrellisReturn));

    CDMRTrellisDecode(tdibits, TrellisReturn);

    for (i = 0; i < pdu_len; i++) //18
    {
      DMR_PDU[i] = (uint8_t)TrellisReturn[i+pdu_start]; 
    }

    for(i = 0, j = 0; i < pdu_len; i++, j+=8)
    {
      DMR_PDU_bits[j + 0] = (TrellisReturn[i+pdu_start] >> 7) & 0x01;
      DMR_PDU_bits[j + 1] = (TrellisReturn[i+pdu_start] >> 6) & 0x01;
      DMR_PDU_bits[j + 2] = (TrellisReturn[i+pdu_start] >> 5) & 0x01;
      DMR_PDU_bits[j + 3] = (TrellisReturn[i+pdu_start] >> 4) & 0x01;
      DMR_PDU_bits[j + 4] = (TrellisReturn[i+pdu_start] >> 3) & 0x01;
      DMR_PDU_bits[j + 5] = (TrellisReturn[i+pdu_start] >> 2) & 0x01;
      DMR_PDU_bits[j + 6] = (TrellisReturn[i+pdu_start] >> 1) & 0x01;
      DMR_PDU_bits[j + 7] = (TrellisReturn[i+pdu_start] >> 0) & 0x01;
    }

    //TODO: run CRC9 on confirmed 3/4 data blocks

  }

  if (is_full) //Rate 1 Data
  {
    CRCExtracted = 0;
    CRCComputed = 0;
    IrrecoverableErrors = 0;

    //TODO: run CRC9 on confirmed data

    for (i = 0; i < 24; i++)
    {
      DMR_PDU[i] = (uint8_t)ConvertBitIntoBytes(&info[i*8], 8);
    }
    //leftover 4 bits, manual doesn't say which 4 so assuming last 4
    //pushing last 4 to 25 and shifting it
    DMR_PDU[25] = (uint8_t)ConvertBitIntoBytes(&info[192], 4);
    DMR_PDU[25] = DMR_PDU[25] << 4;

    //copy info to dmr_pdu_bits
    memcpy(DMR_PDU_bits, info, sizeof(DMR_PDU_bits));

  }

  
  //time for some pi
  if (databurst == 0x00) dmr_pi (opts, state, DMR_PDU, CRCCorrect, IrrecoverableErrors);

  //full link control
  if (databurst == 0x01) dmr_flco (opts, state, DMR_PDU_bits, CRCCorrect, IrrecoverableErrors, 1); //VLC
  if (databurst == 0x02) dmr_flco (opts, state, DMR_PDU_bits, CRCCorrect, IrrecoverableErrors, 2); //TLC
  if (databurst == 0xEB) dmr_flco (opts, state, DMR_PDU_bits, CRCCorrect, IrrecoverableErrors, 3); //EMB

  //dmr data header and multi block types (header, 1/2, 3/4, 1, Unified) - type 1
  if (databurst == 0x06) dmr_dheader (opts, state, DMR_PDU, CRCCorrect, IrrecoverableErrors);
  if (databurst == 0x07) dmr_block_assembler (opts, state, DMR_PDU, pdu_len, databurst, 1); //1/2 Rate Data
  if (databurst == 0x08) dmr_block_assembler (opts, state, DMR_PDU, pdu_len, databurst, 1); //3/4 Rate Data
  //if (databurst == 0x0A) dmr_block_assembler (opts, state, DMR_PDU, pdu_len, databurst, 1); //Full Rate Data - Disabled until further testing can be done

  //is unified data handled like normal data, or control signalling data?
  if (databurst == 0x0B) dmr_block_assembler (opts, state, DMR_PDU, pdu_len, databurst, 1); //Unified Data
  //if (databurst == 0x0B) dmr_cspdu (opts, state, DMR_PDU_bits, DMR_PDU, CRCCorrect, IrrecoverableErrors);

  //control signalling types (CSBK, MBC, UDT)
  if (databurst == 0x03) dmr_cspdu (opts, state, DMR_PDU_bits, DMR_PDU, CRCCorrect, IrrecoverableErrors);
  
  //both MBC header and MBC continuation will go to the block_assembler - type 2, and then to dmr_cspdu 
  if (databurst == 0x04) 
  {
    state->data_block_counter[slot] = 0; //zero block counter before running header
    dmr_block_assembler (opts, state, DMR_PDU, pdu_len, databurst, 2);
  } 
  if (databurst == 0x05) dmr_block_assembler (opts, state, DMR_PDU, pdu_len, databurst, 2);

  //print the unified PDU format here, if not slot idle
  if (opts->payload == 1 && databurst != 0x09)
  {
    fprintf (stderr, "\n");
    fprintf (stderr, "%s", KCYN);
    fprintf (stderr, " DMR PDU Payload ");
    for (i = 0; i < pdu_len; i++)
    {
      fprintf (stderr, "[%02X]", DMR_PDU[i]);
    }

    fprintf (stderr, "%s", KNRM);

    // fprintf (stderr, "%s", KCYN);
    // fprintf (stderr, "\n  Hex to Ascii - ");
    // for (i = 0; i < 12; i++)
    // {
    //   if (DMR_PDU[i] <= 0x7E && DMR_PDU[i] >=0x20)
    //   {
    //     fprintf (stderr, "%c", DMR_PDU[i]);
    //   }
    //   else fprintf (stderr, ".");
    // }
    
    fprintf (stderr,"%s", KNRM);
  }

}

