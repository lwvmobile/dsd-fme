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

//WIP:  Unified Single Block Data - USBD -- TODO: USBD LIP Protocol Handling
//WIP:  UDT Header and Blocks (coded - need samples)
//WIP:  CRC9/CRC32 on Rate 1 Data (need samples)
//WIP:  Reverse Channel Signalling - RC single burst BPTC/7-bit CRC (rc interleave) and when to run RC and not SB
//WIP:  Single Burst Embedded LC - Non-RC single burst LC - Look for Late Entry Alg/Key

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

  //confirmed data 
  uint8_t dbsn = 0; //data block serial number for confirmed data blocks
  uint8_t blockcounter = 0; //local block count
  uint8_t confdatabits[250]; //array to reshuffle conf data block bits into sequence for crc9 check
  memset (confdatabits, 0, sizeof(confdatabits));

  //BPTC 196x96 Specific
  uint8_t  BPTCDeInteleavedData[196];
  uint8_t  BPTCDmrDataBit[96];
  uint8_t  BPTCDmrDataByte[12];

  //Embedded Signalling Specific
  uint8_t  BptcDataMatrix[8][16];
  uint8_t  LC_DataBit[77];
  uint8_t  LC_DataBytes[10];
  int Burst = -1;

  memset (BPTCDeInteleavedData, 0, sizeof(BPTCDeInteleavedData));
  memset (BPTCDmrDataBit, 0, sizeof(BPTCDmrDataBit));
  memset (BPTCDmrDataByte, 0, sizeof(BPTCDmrDataByte));
  memset (BptcDataMatrix, 0, sizeof(BptcDataMatrix));
  memset (LC_DataBit, 0, sizeof(LC_DataBit));
  memset (LC_DataBytes, 0, sizeof(LC_DataBytes));

  //PDU Bytes and Bits
  uint8_t  DMR_PDU[25]; 
  uint8_t  DMR_PDU_bits[196]; 
  memset (DMR_PDU, 0, sizeof (DMR_PDU));
  memset (DMR_PDU_bits, 0, sizeof (DMR_PDU_bits));

  uint8_t  R[3];
  uint8_t  BPTCReservedBits = 0;
  uint8_t  is_ras = 0;
  uint8_t  crc_original_validity = 0;

  uint32_t crcmask = 0; 
  uint8_t  crclen = 0;

  uint8_t is_bptc = 0;
  uint8_t is_trellis = 0; 
  uint8_t is_emb = 0;
  uint8_t is_lc = 0;  
  uint8_t is_full = 0;
  uint8_t is_udt = 0; 
  uint8_t pdu_len = 0;
  uint8_t pdu_start = 0; //starting value of pdu (0 normal, 2 for confirmed)

  uint8_t usbd_st = 0; //usbd service type 

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
    case 0x07: //1/2 Rate Data
      is_bptc = 1;
      crclen = 9; //confirmed data only
      crcmask = 0x0F0; 
      pdu_len = 12; //12 bytes unconfirmed
      sprintf(state->fsubtype, " R12U ");
      if (state->data_conf_data[slot] == 1)
      {
        pdu_len = 10; 
        pdu_start = 2; //start at plus two when assembling
        sprintf(state->fsubtype, " R12C ");
      }
      if (state->data_header_format[slot] == 0) //UDT 1/2 Encoded Blocks
      {
        is_udt = 1; 
        sprintf(state->fsubtype, " UDTC ");
      }
      break;
    case 0x08: //3/4 Rate Data
      is_trellis = 1;
      crclen = 9; //confirmed data only
      crcmask = 0x1FF;
      pdu_len = 18; //18 bytes unconfirmed
      sprintf(state->fsubtype, " R34U ");
      if (state->data_conf_data[slot] == 1)
      {
        pdu_len = 16; 
        pdu_start = 2; //start at plus two when assembling
        sprintf(state->fsubtype, " R34C ");
      } 
      break;
    case 0x09: //Idle
      //pseudo-random data fill, no need to do anything with this
      sprintf(state->fsubtype, " IDLE ");
      break;
    case 0x0A: //1 Rate Data
      crclen = 9; //confirmed data only
      crcmask = 0x10F; 
      is_full = 1;
      pdu_len = 25; //196 bits - 24.5 bytes
      sprintf(state->fsubtype, " R_1U ");
      if (state->data_conf_data[slot] == 1)
      {
        pdu_len = 23; 
        pdu_start = 2; //start at plus two when assembling
        sprintf(state->fsubtype, " R_1C ");
      }  
      break;
    case 0x0B: //Unified Single Block Data USBD
      is_bptc = 1;
      crclen = 16;
      crcmask = 0x3333;
      pdu_len = 12; //12 bytes
      sprintf(state->fsubtype, " USBD ");
      break;
      
    //special types (not real data 'sync' bursts)
    case 0xEB: //Embedded Signalling
      crclen = 5;
      is_emb = 1;
      pdu_len = 9;
      break;

    default:
      //Slot Type FEC should catch this so we never see it,
      //but if it doesn't, then we can still dump the entire 'packet'
      //treat like rate 1 unconfirmed data 
      is_full = 1;
      pdu_len = 25; //196 bits - 24.5 bytes 
      sprintf(state->fsubtype, " _UNK "); 
      break;
    
  }

  //flag off prop head when not looking at data blocks
  if (databurst != 0x6 && databurst != 0x7 && databurst != 0x8 && databurst != 0xA && databurst != 0xB) state->data_p_head[slot] = 0;

  if (databurst != 0xEB)
  {
    if (state->dmr_ms_mode == 0) fprintf(stderr, "| Color Code=%02d ", state->dmr_color_code);
    fprintf(stderr, "|%s", state->fsubtype);

    //'DSP' output to file
    if (opts->use_dsp_output == 1)
    {
      FILE * pFile; //file pointer
      pFile = fopen (opts->dsp_out_file, "a");
      fprintf (pFile, "\n%d 98 ", slot+1); //'98' is CACH designation value
      for (i = 0; i < 6; i++) //3 byte CACH
      {
        int cach_byte = (state->dmr_stereo_payload[i*2] << 2) | state->dmr_stereo_payload[i*2 + 1];
        fprintf (pFile, "%X", cach_byte); //nibble, not a byte, next time I look at this and wonder why its not %02X
      }
      fprintf (pFile, "\n%d %02X ", slot+1, databurst); //use hex value of current data burst type
      for (i = 6; i < 72; i++) //33 bytes, no CACH
      {
        int dsp_byte = (state->dmr_stereo_payload[i*2] << 2) | state->dmr_stereo_payload[i*2 + 1];
        fprintf (pFile, "%X", dsp_byte);
      }
      fclose (pFile);
    }
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
    BPTCReservedBits = (R[0] & 0x01) | ((R[1] << 1) & 0x02) | ((R[2] << 2) & 0x04);

    //debug print
    //fprintf (stderr, " RAS? %X - %d %d %d", BPTCReservedBits, R[0], R[1], R[2]); 

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

    //set CRC to correct on unconfirmed 1/2 data blocks (for reporting due to no CRC available on these)
    else if (state->data_conf_data[slot] == 0 && databurst == 0x7)
    {
      CRCComputed = 0;
      CRCExtracted = 0;
      CRCCorrect = 1;
    } 

    //run CRC9 on intermediate and last 1/2 confirmed data blocks
    else if (state->data_conf_data[slot] == 1 && databurst == 0x7)
    {
      blockcounter = state->data_block_counter[slot]; //current block number according to the counter
      dbsn = (uint32_t)ConvertBitIntoBytes(&BPTCDmrDataBit[0], 7) + 1; //recover data block serial number
      CRCExtracted = (uint32_t)ConvertBitIntoBytes(&BPTCDmrDataBit[7], 9); //extract CRC from data
      CRCExtracted = CRCExtracted ^ crcmask;

      //reorganize the BPTCDmrDataBit array into confdatabits, just for CRC9 check
      //need to find a sample to confirm this
      for(i = 0; i < 80; i++) confdatabits[i] = BPTCDmrDataBit[i + 16];
      for(i = 0; i < 7; i++) confdatabits[i + 80] = BPTCDmrDataBit[i];

      CRCComputed = ComputeCrc9Bit(confdatabits, 87);
      if (CRCExtracted == CRCComputed)
      {
        CRCCorrect = 1;
        state->data_block_crc_valid[slot][blockcounter] = 1;
      } 
      else state->data_block_crc_valid[slot][blockcounter] = 0;

    }

    //run CCITT on other data forms
    else 
    {
      CRCComputed = ComputeCrcCCITT(BPTCDmrDataBit); 
      if (CRCComputed == CRCExtracted) CRCCorrect = 1;
      else CRCCorrect = 0;
    }

    //set the 'RAS Flag', if no irrecoverable errors but bad crc, only when enabled by user (to prevent a lot of bad data CSBKs)
    if (opts->aggressive_framesync == 0 && CRCCorrect == 0 && IrrecoverableErrors == 0 && BPTCReservedBits == 4) is_ras = 1; 

    //make sure the system type isn't Hytera, but could just be bad decodes on bad sample
    if (BPTCDmrDataByte[1] == 0x68) is_ras = 0;

    //if this is a RAS system, set the CRC to okay if irrecoverable errors are okay
    //if we don't do this, then we can't view some data (CSBKs on RAS enabled systems) 
    if (is_ras == 1)
    {
      crc_original_validity = CRCCorrect;
      CRCCorrect = 1;
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

    //convert to DMR_PDU and DMR_PDU_bits
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
    CRCExtracted = (uint32_t)ConvertBitIntoBytes(&LC_DataBit[72], 5);

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

  //the sexy one
  if (is_trellis)
  {
    CRCExtracted = 0;
    CRCComputed = 0;
    IrrecoverableErrors = 1; 

    uint8_t tdibits[98];
    memset (tdibits, 0, sizeof(tdibits));

    //reconstitute info bits into reverse ordered dibits for the trellis decoder
    for (i = 0; i < 98; i++)
      tdibits[i] = (info[i*2] << 1) | info[i*2+1]; 

    uint8_t TrellisReturn[18];
    memset (TrellisReturn, 0, sizeof(TrellisReturn));
    IrrecoverableErrors = dmr_34(tdibits, TrellisReturn);

    //debug -- its a lot easier to hear the number of errors, than to eyeball it
    // if (IrrecoverableErrors) beeper (opts, state, slot);

    for (i = 0; i < pdu_len; i++) //18
      DMR_PDU[i] = TrellisReturn[i+pdu_start]; 


    for(i = 0, j = 0; i < 18; i++, j+=8)
    {
      DMR_PDU_bits[j + 0] = (TrellisReturn[i] >> 7) & 0x01;
      DMR_PDU_bits[j + 1] = (TrellisReturn[i] >> 6) & 0x01;
      DMR_PDU_bits[j + 2] = (TrellisReturn[i] >> 5) & 0x01;
      DMR_PDU_bits[j + 3] = (TrellisReturn[i] >> 4) & 0x01;
      DMR_PDU_bits[j + 4] = (TrellisReturn[i] >> 3) & 0x01;
      DMR_PDU_bits[j + 5] = (TrellisReturn[i] >> 2) & 0x01;
      DMR_PDU_bits[j + 6] = (TrellisReturn[i] >> 1) & 0x01;
      DMR_PDU_bits[j + 7] = (TrellisReturn[i] >> 0) & 0x01;
    }

    //set CRC to correct on unconfirmed 3/4 data blocks (for reporting due to no CRC available on these)
    if (state->data_conf_data[slot] == 0) CRCCorrect = 1;
 
    //run CRC9 on intermediate and last 3/4 data blocks
    else if (state->data_conf_data[slot] == 1)
    {
      blockcounter = state->data_block_counter[slot]; //current block number according to the counter
      dbsn = (uint32_t)ConvertBitIntoBytes(&DMR_PDU_bits[0], 7) + 1; //recover data block serial number
      CRCExtracted = (uint32_t)ConvertBitIntoBytes(&DMR_PDU_bits[7], 9); //extract CRC from data
      CRCExtracted = CRCExtracted ^ crcmask;

      //reorganize the DMR_PDU_bits array into confdatabits, just for CRC9 check
      //confirmed working now!
      for(i = 0; i < 128; i++) confdatabits[i] = DMR_PDU_bits[i + 16];
      for(i = 0; i < 7; i++) confdatabits[i + 128] = DMR_PDU_bits[i];

      CRCComputed = ComputeCrc9Bit(confdatabits, 135); 
      if (CRCExtracted == CRCComputed)
      {
        CRCCorrect = 1;
        state->data_block_crc_valid[slot][blockcounter] = 1;
      } 
      else
      {
        
        state->data_block_crc_valid[slot][blockcounter] = 0;
      } 

    }

    //reorganize the DMR_PDU_bits into PDU friendly format (minus the dbsn and crc9)
    memset (DMR_PDU_bits, 0, sizeof(DMR_PDU_bits));
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

  }

  if (is_full) //Rate 1 Data -- todo: fix padding bits, the four padding bits are on bit 96,97,98,99
  {
    CRCExtracted = 0;
    CRCComputed = 0;
    IrrecoverableErrors = 0; //implicit, since there is no encoding

    for (i = 0; i < 24; i++)
    {
      DMR_PDU[i] = (uint8_t)ConvertBitIntoBytes(&info[i*8], 8);
    }
    //leftover 4 bits, manual doesn't say which 4 so assuming last 4
    //pushing last 4 to 25 and shifting it
    DMR_PDU[25] = (uint8_t)ConvertBitIntoBytes(&info[192], 4);
    DMR_PDU[25] = DMR_PDU[25] << 4;

    //set CRC to correct on unconfirmed 1 rate data blocks (for reporting due to no CRC available on these)
    if (state->data_conf_data[slot] == 0) CRCCorrect = 1;
 
    //run CRC9 on intermediate and last 1 rate data blocks
    else if (state->data_conf_data[slot] == 1)
    {
      blockcounter = state->data_block_counter[slot]; //current block number according to the counter
      dbsn = (uint32_t)ConvertBitIntoBytes(&info[0], 7) + 1; //recover data block serial number
      CRCExtracted = (uint32_t)ConvertBitIntoBytes(&info[7], 9); //extract CRC from data
      CRCExtracted = CRCExtracted ^ crcmask;

      //reorganize the info bit array into confdatabits, just for CRC9 check
      //unconfirmed, haven't seen any samples with rate 1 data
      for(i = 0; i < 176; i++) confdatabits[i] = info[i + 16];
      for(i = 0; i < 7; i++) confdatabits[i + 176] = info[i];

      CRCComputed = ComputeCrc9Bit(confdatabits, 183); 
      if (CRCExtracted == CRCComputed)
      {
        CRCCorrect = 1;
        state->data_block_crc_valid[slot][blockcounter] = 1;
      } 
      else state->data_block_crc_valid[slot][blockcounter] = 0;

    }

    //copy info to dmr_pdu_bits
    memcpy(DMR_PDU_bits, info, sizeof(DMR_PDU_bits));

  }

  
  //time for some pi
  if (databurst == 0x00) dmr_pi (opts, state, DMR_PDU, CRCCorrect, IrrecoverableErrors);

  //full link control
  if (databurst == 0x01) dmr_flco (opts, state, DMR_PDU_bits, CRCCorrect, IrrecoverableErrors, 1); //VLC
  if (databurst == 0x02) dmr_flco (opts, state, DMR_PDU_bits, CRCCorrect, IrrecoverableErrors, 2); //TLC
  if (databurst == 0xEB) dmr_flco (opts, state, DMR_PDU_bits, CRCCorrect, IrrecoverableErrors, 3); //EMB

  //dmr data header and multi block types (header, 1/2, 3/4, 1, UDT) - type 1
  if (databurst == 0x06) dmr_dheader (opts, state, DMR_PDU, DMR_PDU_bits, CRCCorrect, IrrecoverableErrors);
  if (databurst == 0x08) dmr_block_assembler (opts, state, DMR_PDU, pdu_len, databurst, 1); //3/4 Rate Data
  if (databurst == 0x0A) dmr_block_assembler (opts, state, DMR_PDU, pdu_len, databurst, 1); //Full Rate Data
  if (databurst == 0x07 && is_udt == 0) dmr_block_assembler (opts, state, DMR_PDU, pdu_len, databurst, 1); //1/2 Rate Data
  if (databurst == 0x07 && is_udt == 1) dmr_block_assembler (opts, state, DMR_PDU, pdu_len, databurst, 3); //UDT with 1/2 Rate Encoding

  //control signalling types (CSBK, MBC)
  if (databurst == 0x03) dmr_cspdu (opts, state, DMR_PDU_bits, DMR_PDU, CRCCorrect, IrrecoverableErrors);
  
  //both MBC header and MBC continuation will go to the block_assembler - type 2, and then to dmr_cspdu 
  if (databurst == 0x04) 
  {
    state->data_block_counter[slot] = 0; //zero block counter before running header
    state->data_header_valid[slot] = 1; //set valid header since we received one
    dmr_block_assembler (opts, state, DMR_PDU, pdu_len, databurst, 2);
  } 
  if (databurst == 0x05) dmr_block_assembler (opts, state, DMR_PDU, pdu_len, databurst, 2);

  //Unified Single Data Block (USBD) -- Not to be confused with Unified Data Transport (UDT)
  if (databurst == 0x0B)
  {
    //TODO: provide more robust handling at a later date -- ETSI TS 102 361-4 V1.11.1 (2021-01) 6.6.11.3
    usbd_st = (uint8_t)ConvertBitIntoBytes(&DMR_PDU_bits[0], 4);
    fprintf (stderr, "%s\n", KYEL);
    fprintf (stderr, " Unified Single Block Data - ");
    if (usbd_st == 0) fprintf (stderr, "Location Information Protocol");
    else if (usbd_st > 8) fprintf (stderr, "Manufacturer Specific Service %d ", usbd_st);
    else fprintf (stderr, "Reserved %d ", usbd_st);
  }

  //set the original CRCCorrect back to its original value if the RAS flag was tripped
  if (is_ras == 1) CRCCorrect = crc_original_validity;

  //start printing relevant fec/crc/ras messages, don't print on idle or MBC continuation blocks (handled in dmr_block.c)
  if (IrrecoverableErrors == 0 && CRCCorrect == 1 && databurst != 0x09 && databurst != 0x05) ; //fprintf(stderr, "(CRC OK)");

  // if (IrrecoverableErrors == 0 && CRCCorrect == 0 && databurst != 0x09 && databurst != 0x05)
  // {
  //   fprintf (stderr, "%s", KYEL);
  //   fprintf(stderr, " (FEC OK)");
  //   fprintf (stderr, "%s", KNRM);

  // } 

  if (IrrecoverableErrors != 0 && databurst != 0x09) //&& databurst != 0x05
  {
    fprintf (stderr, "%s", KRED);
    fprintf(stderr, " (FEC ERR)");
    fprintf (stderr, "%s", KNRM);
  } 

  //print whether or not the 'RAS Field' bits are set to indicate RAS enabled (to be verified)
  if (is_ras == 1)
  {
    fprintf (stderr, "%s", KRED);
    fprintf (stderr, " -RAS ");
    //the value of this field seems to always, or usually, be 4, or just R[2] bit is set
    if (opts->payload == 1) fprintf (stderr, "%X ", BPTCReservedBits);
    fprintf (stderr, "%s", KNRM);
  }
  
  if (IrrecoverableErrors == 0 && CRCCorrect == 0 && is_ras == 0 && databurst != 0x09 && databurst != 0x05)
  {
    fprintf (stderr, "%s", KRED);
    fprintf (stderr, " (CRC ERR) ");
    fprintf (stderr, "%s", KNRM);
  }

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

    //debug print
    // if (dbsn) fprintf (stderr, " SN %X", dbsn);
    // fprintf (stderr, " CRC - EXT %X CMP %X", CRCExtracted, CRCComputed);

    fprintf (stderr, "%s", KNRM);
    
  }

}

