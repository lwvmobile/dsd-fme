/*-------------------------------------------------------------------------------
 * dmr_block.c
 * DMR Data Header and Data Block Assembly/Handling
 *
 * LWVMOBILE
 * 2022-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

void dmr_dheader (dsd_opts * opts, dsd_state * state, uint8_t dheader[], uint32_t CRCCorrect, uint32_t IrrecoverableErrors)
{
  uint32_t i, j, k;
  uint8_t slot = state->currentslot;

  //clear out unified pdu 'superframe' slot
  for (short int i = 0; i < 288; i++) 
  {
    state->dmr_pdu_sf[slot][i] = 0;
  }

  //TODO: ONLY RUN ON GOOD CRC?

  //collect relevant data header info, figure out format, and grab appropriate info from it
  if (state->data_p_head[slot] == 0)
  {
    fprintf (stderr, "%s ", KGRN);
    state->data_block_counter[slot] = 1; //reset block counter to 1
    state->data_header_format[slot] = dheader[0]; //collect the entire byte, we can check group or individual, response requested as well on this
    state->data_header_sap[slot] = (dheader[1] & 0xF0) >> 4; 

    if ( (state->data_header_format[slot] & 0xF) ==  0x2)
    {
      state->data_conf_data[slot] = 0; //flag as unconfirmed data, no need for additional CRC/Serial checks
      state->data_header_padding[slot] = dheader[1] & 0xF; //number of padding octets on last block
      state->data_header_blocks[slot] = dheader[8] & 0x7F; //number of blocks to follow, most useful for when to poll the super array for data
      fprintf (stderr, "\n  Unconfirmed Data Header: DPF %01X - SAP %02X - Block Count %02X - Padding Octets %02X",
               state->data_header_format[slot] & 0xF, state->data_header_sap[slot],
               state->data_header_blocks[slot], state->data_header_padding[slot] );
    }

    if ( (state->data_header_format[slot] & 0xF) ==  0x3)
    {
      state->data_conf_data[slot] = 1; //set flag to indicate the following blocks are confirmed data so we know to run additional CRC etc on it.
      state->data_header_padding[slot] = dheader[1] & 0xF; //number of padding octets on last block
      state->data_header_blocks[slot] = dheader[8] & 0x7F; //number of blocks to follow, most useful for when to poll the super array for data
      fprintf (stderr, "\n  Confirmed Data Header: DPF %01X - SAP %02X - Block Count %02X - Padding Octets %02X",
               state->data_header_format[slot] & 0xF, state->data_header_sap[slot],
               state->data_header_blocks[slot], state->data_header_padding[slot] );
    }

    if ( (state->data_header_format[slot] & 0xF) ==  0xD) //DD_HEAD
    {
      state->data_conf_data[slot] = 0; 
      state->data_header_padding[slot] = dheader[7] & 0xF;
      state->data_header_blocks[slot] = ( (dheader[0] & 0x30) | ((dheader[1] & 0xF) >> 0) ); //what a pain MSB | LSB
      fprintf (stderr, "\n  Short Data - Defined: DPF %01X - SAP %02X - Appended Blocks %02X - Padding Bits %02X",
               state->data_header_format[slot] & 0xF, state->data_header_sap[slot],
               state->data_header_blocks[slot], state->data_header_padding[slot] );
    }

    if ( (state->data_header_format[slot] & 0xF) ==  0xE) //SP_HEAD or R_HEAD
    {
      state->data_conf_data[slot] = 0; 
      state->data_header_padding[slot] = dheader[7] & 0xF;
      state->data_header_blocks[slot] = ( (dheader[0] & 0x30) | ((dheader[1] & 0xF) >> 0) ); 
      fprintf (stderr, "\n  Short Data - Raw or Precoded: DPF %01X - SAP %02X - Appended Blocks %02X - Padding Bits %02X",
               state->data_header_format[slot] & 0xF, state->data_header_sap[slot],
               state->data_header_blocks[slot], state->data_header_padding[slot] );
    }


  }

  //do we still need this? since we have ample PDU storage
  if (state->data_header_blocks[slot] > 7)
  {
    state->data_header_blocks[slot] = 7;
  }

  fprintf (stderr, "%s ", KNRM);
  //end collecting data header info

  fprintf (stderr, "%s ", KMAG);

  if (state->data_header_sap[slot] == 0x4 && state->data_p_head[slot] == 0) 
  {
    fprintf (stderr, "\n  IP Data - Source: ");
    fprintf (stderr, "[%d]", (dheader[5] <<16 ) + (dheader[6] << 8) + dheader[7] );
    fprintf (stderr, " Destination: ");
    fprintf (stderr, "[%d]", (dheader[2] <<16 ) + (dheader[3] <<8 ) + dheader[4] );

    state->dmr_lrrp_source[slot] = (dheader[5] <<16 ) + (dheader[6] << 8) + dheader[7];

  }

  if (state->data_header_sap[slot] == 0x0 && state->data_p_head[slot] == 0) 
  {
    fprintf (stderr, "\n  Unified Data Transport - Source: ");
    fprintf (stderr, "[%d]", (dheader[5] <<16 ) + (dheader[6] << 8) + dheader[7] );
    fprintf (stderr, " Destination: ");
    fprintf (stderr, "[%d]", (dheader[2] <<16 ) + (dheader[3] <<8 ) + dheader[4] );

    state->dmr_lrrp_source[slot] = (dheader[5] <<16 ) + (dheader[6] << 8) + dheader[7];

  }

  if (state->data_header_sap[slot] == 0x9 && state->data_p_head[slot] == 0) 
  {
    fprintf (stderr, "\n  Proprietary Packet Data - Source: ");
    fprintf (stderr, "[%d]", (dheader[5] <<16 ) + (dheader[6] << 8) + dheader[7] );
    fprintf (stderr, " Destination: ");
    fprintf (stderr, "[%d]", (dheader[2] <<16 ) + (dheader[3] <<8 ) + dheader[4] );

    state->dmr_lrrp_source[slot] = (dheader[5] <<16 ) + (dheader[6] << 8) + dheader[7];

  }

  if (state->data_header_sap[slot] == 0x2 && state->data_p_head[slot] == 0) 
  {
    fprintf (stderr, "\n  TCP/IP Header Compression Data - Source: ");
    fprintf (stderr, "[%d]", dheader[3]);
    fprintf (stderr, " Destination: ");
    fprintf (stderr, "[%d]", dheader[2]);

    state->dmr_lrrp_source[slot] = dheader[3];

  }

  if (state->data_header_sap[slot] == 0x3 && state->data_p_head[slot] == 0) 
  {
    fprintf (stderr, "\n  UDP/IP Header Compression Data - Source: ");
    fprintf (stderr, "[%d]", dheader[3]);
    fprintf (stderr, " Destination: ");
    fprintf (stderr, "[%d]", dheader[2]);

    state->dmr_lrrp_source[slot] = dheader[3];

  }

  if (state->data_header_sap[slot] == 0x5 && state->data_p_head[slot] == 0) 
  {
    fprintf (stderr, "\n  ARP - Address Resolution Protocol ");
  }

  if ( (state->data_header_format[slot] & 0xF ) == 0xD && state->data_p_head[slot] == 0) 
  {
    fprintf (stderr, "\n  Short Data - Defined - Source: ");
    fprintf (stderr, "[%d]", (dheader[5] <<16 ) + (dheader[6] << 8) + dheader[7] );
    fprintf (stderr, " Destination: ");
    fprintf (stderr, "[%d]", (dheader[2] <<16 ) + (dheader[3] <<8 ) + dheader[4] );
  }

  if ( (state->data_header_format[slot] & 0xF) == 0xE && state->data_p_head[slot] == 0) 
  {
    fprintf (stderr, "\n  Short Data - Raw or Status/Precoded - Source: ");
    fprintf (stderr, "[%d]", (dheader[5] <<16 ) + (dheader[6] << 8) + dheader[7] );
    fprintf (stderr, " Destination: ");
    fprintf (stderr, "[%d]", (dheader[2] <<16 ) + (dheader[3] <<8 ) + dheader[4] );
  }

  if ( (state->data_header_format[slot] & 0xF ) == 0x1 && state->data_p_head[slot] == 0)
  {
    fprintf (stderr, "\n  Response Packet - Source:  ");
    fprintf (stderr, "[%d]", (dheader[5] <<16 ) + (dheader[6] << 8) + dheader[7] );
    fprintf (stderr, " Destination: ");
    fprintf (stderr, "[%d]", (dheader[2] <<16 ) + (dheader[3] <<8 ) + dheader[4] );
  }

  if ( (state->data_header_format[slot] & 0xF ) == 0x0 && state->data_p_head[slot] == 0) 
  {
    fprintf (stderr, "\n  Unified Data Transport - Source:  ");
    fprintf (stderr, "[%d]", (dheader[5] <<16 ) + (dheader[6] << 8) + dheader[7] );
    fprintf (stderr, " Destination: ");
    fprintf (stderr, "[%d]", (dheader[2] <<16 ) + (dheader[3] <<8 ) + dheader[4] );
  }

  if ( (state->data_header_format[slot] & 0x40) == 0x40 && state->data_p_head[slot] == 0) //check response req bit
  {
    fprintf (stderr, "\n  Response Requested - ");
    fprintf (stderr, " Source:");
    fprintf (stderr, " [%d]", (dheader[2] <<16 ) + (dheader[3] << 8) + dheader[4] );
    fprintf (stderr, " Destination:");
    fprintf (stderr, " [%d]", (dheader[5] <<16 ) + (dheader[6] <<8 ) + dheader[7] );
  }

  //if proprietary header was flagged by the last data header, then process data differently
  if (state->data_p_head[slot] == 1)
  {
    fprintf (stderr, "\n  Proprietary Data Header: SAP %01X - Format %01X - MFID %02X",
             (dheader[0] & 0xF0) >> 4, dheader[0] & 0xF, dheader[1]       );

    state->data_p_head[slot] = 0;
    state->data_header_sap[slot] = 0; //reset this to zero so we don't trip the condition below
    state->data_block_counter[slot]++; //increment the counter since this counts against the data blocks

    //set overarching manufacturer in use when non-standard feature id set is up
    if (dheader[1] != 0) state->dmr_mfid = dheader[1];

    //assign prop header pdu to 'superframe'
    uint8_t blocks = state->data_header_blocks[slot] - 1;
    for(i = 0; i < 12; i++)
    {
      state->dmr_pdu_sf[slot][i+(blocks*12)] = dheader[i];
    }
  }

  //only set this flag after everything else is processed
  if (state->data_header_sap[slot] == 0x9 && state->data_p_head[slot] == 0) 
  {
    fprintf (stderr, "\n  Proprietary Packet Data Incoming ");
    state->data_p_head[slot] = 1; //flag that next data header will be a proprietary data header
  }
  fprintf (stderr, "%s ", KNRM);

  //let user know the data header may have errors
  fprintf (stderr, "%s", KRED);
  if (CRCCorrect == 1) ; //CRCCorrect 1 is good, else is bad CRC; no print on good
  else if(IrrecoverableErrors == 0) fprintf(stderr, " (FEC OK)");
  else fprintf(stderr, " (FEC/CRC ERR)");
  fprintf (stderr, "%s", KNRM);

}

//assemble the blocks as they come in, shuffle them into the unified dmr_pdu_sf
void dmr_block_assembler (dsd_opts * opts, dsd_state * state, uint8_t block_bytes[], uint8_t block_len, uint8_t databurst, uint8_t type)
{
  int i, j, k;
  uint8_t lb = 0; //mbc last block
  uint8_t pf = 0; //mbc protect flag
  uint8_t is_conf_data = 0; //internal flag to see if type 1 blocks contain confirmed data crcs and serials
  uint8_t slot = state->currentslot; 
  int blocks; 
  uint8_t blockcounter = state->data_block_counter[slot];

  //crc specific variables
  uint8_t len = 24; //len of completed message needing crc32 number of blocks*block_len - 12? 
  uint8_t crclen = 32; //len of the crc32 (obviously)
  uint8_t start = 12; //starting position (+12 offset loaded up data header)
  uint8_t n = 12; //number of bytes per block

  //TODO: CRC check on Header, Blocks and full frames as appropriate
  uint32_t CRCCorrect = 0;
  uint32_t CRCComputed = 0;
  uint32_t CRCExtracted = 0;
  uint32_t IrrecoverableErrors = 0;
  uint8_t dmr_pdu_sf_bits[18*8*8]; //8 blocks at 18 bytes at 8 bits //just a large value to have plenty storage space

  if (type == 1) blocks = state->data_header_blocks[slot] - 1; 
  if (type == 2) blocks = state->data_block_counter[slot];

  //sanity check, setting block_len and block values to sane numbers in case of missing header, else could overload array (crash) or print out
  if (blocks < 1) blocks = 1; //changed blocks from uint8_t to int
  if (blocks > 8) blocks = 8;
  if (block_len == 0) block_len = 18;
  if (block_len > 24) block_len = 24;

  //TODO: add CRC/FEC validation to each block and each confirmed data block as well - type 1
  if (type == 1)
  {
    //type 1 data block, shuffle method
    for (j = 0; j < blocks; j++) //6
    {
      for (i = 0; i < block_len; i++) //block_len
      {
        state->dmr_pdu_sf[slot][i+block_len*j] = state->dmr_pdu_sf[slot][i+block_len*(j+1)];
      }
    }

    for (i = 0; i < block_len; i++) 
    {
      state->dmr_pdu_sf[slot][i+(blocks*block_len)] = block_bytes[i]; 
    }
    //time to send the completed 'superframe' to the DMR PDU message handler
    if (state->data_block_counter[slot] == state->data_header_blocks[slot])
    {
      //TODO: CRC32 on completed messages
      dmr_pdu (opts, state, state->dmr_pdu_sf[slot]);
    }
  }
  

  //type 2 - MBC header and MBC continuation blocks
  if (type == 2) 
  {
    //sanity check (marginal signal, bad decodes, etc)
    if (state->data_block_counter[slot] > 8) state->data_block_counter[slot] = 8;

    //Type 2 data block, additive method
    for (i = 0; i < block_len; i++)
    {
      state->dmr_pdu_sf[slot][i+(blockcounter*block_len)] = block_bytes[i]; 
    }
  
    memset (dmr_pdu_sf_bits, 0, sizeof(dmr_pdu_sf_bits));

    lb = block_bytes[0] >> 7; //last block flag
    pf = (block_bytes[0] >> 6) & 1;

    if (lb == 1) //last block arrived, time to send to cspdu decoder
    {
      for(i = 0, j = 0; i < 12*4; i++, j+=8) //
      {
        dmr_pdu_sf_bits[j + 0] = (state->dmr_pdu_sf[slot][i] >> 7) & 0x01;
        dmr_pdu_sf_bits[j + 1] = (state->dmr_pdu_sf[slot][i] >> 6) & 0x01;
        dmr_pdu_sf_bits[j + 2] = (state->dmr_pdu_sf[slot][i] >> 5) & 0x01;
        dmr_pdu_sf_bits[j + 3] = (state->dmr_pdu_sf[slot][i] >> 4) & 0x01;
        dmr_pdu_sf_bits[j + 4] = (state->dmr_pdu_sf[slot][i] >> 3) & 0x01;
        dmr_pdu_sf_bits[j + 5] = (state->dmr_pdu_sf[slot][i] >> 2) & 0x01;
        dmr_pdu_sf_bits[j + 6] = (state->dmr_pdu_sf[slot][i] >> 1) & 0x01;
        dmr_pdu_sf_bits[j + 7] = (state->dmr_pdu_sf[slot][i] >> 0) & 0x01;
      }

      //TODO: CRC check on Header and full frames as appropriate

      //shim these values for now
      CRCCorrect = 1;
      IrrecoverableErrors = 0;

      dmr_cspdu (opts, state, dmr_pdu_sf_bits, state->dmr_pdu_sf[slot], CRCCorrect, IrrecoverableErrors);

    }
  }

  //Full Super Frame Type 1 - Debug Output
  if (opts->payload == 1 && type == 1 && state->data_block_counter[slot] == state->data_header_blocks[slot]) 
  {
    fprintf (stderr, "%s",KGRN);
    fprintf (stderr, "\n Multi Block PDU Superframe - Slot [%d]\n  ", slot+1);
    for (i = 0; i < ((blocks+1)*block_len); i++) 
    {
      fprintf (stderr, "[%02X]", state->dmr_pdu_sf[slot][i]);
      if (i == 11 || i == 23 || i == 35 || i == 47 || i == 59 || i == 71 || i == 83 || i == 95) 
      {
        fprintf (stderr, "\n  "); 
      }
    }
    fprintf (stderr, "%s ", KNRM);
  }

  //Full Super Frame MBC - Debug Output
  if (opts->payload == 1 && type == 2 && lb == 1)
  {
    fprintf (stderr, "%s",KGRN);
    fprintf (stderr, "\n MBC Multi Block PDU Superframe - Slot [%d]\n  ", slot+1);
    for (i = 0; i < ((blocks+1)*block_len); i++) 
    {
      fprintf (stderr, "[%02X]", state->dmr_pdu_sf[slot][i]);
      if (i == 11 || i == 23 || i == 35 || i == 47 || i == 59 || i == 71 || i == 83 || i == 95) 
      {
        fprintf (stderr, "\n  "); 
      }
    }
    fprintf (stderr, "%s ", KNRM);
  }

  state->data_block_counter[slot]++; //increment block counter

  if (type == 2 && lb == 1)
  {
    state->data_block_counter[slot] = 0; //reset the block counter (MBCs)

    //clear out unified pdu 'superframe' slot
    for (short int i = 0; i < 288; i++)
    {
      state->dmr_pdu_sf[slot][i] = 0;
    }

    state->data_block_crc_valid[slot][0] = 0; //Zero Out MBC Header Block CRC Valid

  }

}