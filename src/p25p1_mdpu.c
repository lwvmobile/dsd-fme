/*-------------------------------------------------------------------------------
 * p25p1_mpdu.c
 * P25p1 Multi Block PDU and Multi Block Trunking
 *
 * LWVMOBILE
 * 2024-12 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

void processRSP(uint8_t C, uint8_t T, uint8_t S, char * rsp_string)
{

  if      (C == 0)  sprintf (rsp_string, " ACK (Success);");
  else if (C == 2)  sprintf (rsp_string, " SACK (Retry);");
  else if (C == 1)
  {
    if      (T == 0) sprintf (rsp_string, " NACK (Illegal Format);");
    else if (T == 1) sprintf (rsp_string, " NACK (CRC32 Failure);");
    else if (T == 2) sprintf (rsp_string, " NACK (Memory Full);");
    else if (T == 3) sprintf (rsp_string, " NACK (FSN Sequence Error);");
    else if (T == 4) sprintf (rsp_string, " NACK (Undeliverable);");
    else if (T == 5) sprintf (rsp_string, " NACK (NS/VR Sequence Error);"); //depreciated
    else if (T == 6) sprintf (rsp_string, " NACK (Invalid User on System);");
  }
  
  fprintf (stderr, " Response Packet:%s C: %X; T: %X; S: %X; ", rsp_string, C, T, S);

}

void processSAP(uint8_t SAP, char * sap_string)
{

  if      (SAP == 0)  sprintf (sap_string, " User Data;");
  else if (SAP == 1)  sprintf (sap_string, " Encrypted User Data;");
  else if (SAP == 2)  sprintf (sap_string, " Circuit Data;");
  else if (SAP == 3)  sprintf (sap_string, " Circuit Data Control;");
  else if (SAP == 4)  sprintf (sap_string, " Packet Data;");
  else if (SAP == 5)  sprintf (sap_string, " Address Resolution Protocol;");
  else if (SAP == 6)  sprintf (sap_string, " SNDCP Packet Data Control;");
  else if (SAP == 15) sprintf (sap_string, " Packet Data Scan Preamble;");
  else if (SAP == 29) sprintf (sap_string, " Packet Data Encryption Support;");
  else if (SAP == 31) sprintf (sap_string, " Extended Address;");
  else if (SAP == 32) sprintf (sap_string, " Registration and Authorization;");
  else if (SAP == 33) sprintf (sap_string, " Channel Reassignment;");
  else if (SAP == 34) sprintf (sap_string, " System Configuration;");
  else if (SAP == 35) sprintf (sap_string, " Mobile Radio Loopback;");
  else if (SAP == 36) sprintf (sap_string, " Mobile Radio Statistics;");
  else if (SAP == 37) sprintf (sap_string, " Mobile Radio Out of Service;");
  else if (SAP == 38) sprintf (sap_string, " Mobile Radio Paging;");
  else if (SAP == 39) sprintf (sap_string, " Mobile Radio Configuration;");
  else if (SAP == 40) sprintf (sap_string, " Unencrypted Key Management;");
  else if (SAP == 41) sprintf (sap_string, " Encrypted Key Management;");
  else if (SAP == 48) sprintf (sap_string, " Location Service;");
  else if (SAP == 61) sprintf (sap_string, " Trunking Control;");
  else if (SAP == 63) sprintf (sap_string, " Encrypted Trunking Control;");
 
  //catch all for everything else
  else                sprintf (sap_string, " Unknown SAP;");

  fprintf (stderr, "SAP: 0x%02X;%s ", SAP, sap_string);

}

// static uint32_t crc32mbf(uint8_t buf[], int len)
static uint32_t crc32mbf(uint8_t * buf, int len)
{	
  uint32_t g = 0x04c11db7;
  uint64_t crc = 0;
  for (int i = 0; i < len; i++)
  {
    crc <<= 1;
    int b = ( buf [i / 8] >> (7 - (i % 8)) ) & 1;
    if (((crc >> 32) ^ b) & 1) crc ^= g;
  }
  crc = (crc & 0xffffffff) ^ 0xffffffff;
  return crc;
}

void processMPDU(dsd_opts * opts, dsd_state * state)
{

  //p25p2 18v reset counters and buffers
  state->voice_counter[0] = 0; //reset
  state->voice_counter[1] = 0; //reset
  memset (state->s_l4, 0, sizeof(state->s_l4));
  memset (state->s_r4, 0, sizeof(state->s_r4));
  opts->slot_preference = 2;

  //reset some strings when returning from a call in case they didn't get zipped already
  sprintf (state->call_string[0], "%s", "                     "); //21 spaces
  sprintf (state->call_string[1], "%s", "                     "); //21 spaces

  //clear stale Active Channel messages here
  if ( (time(NULL) - state->last_active_time) > 3 )
  {
    memset (state->active_channel, 0, sizeof(state->active_channel));
  }

  uint8_t tsbk_dibit[98];
  memset (tsbk_dibit, 0, sizeof(tsbk_dibit));

  int dibit = 0;
  int r34 = 0;

  uint8_t tsbk_byte[12]; //12 byte return from p25_12
  memset (tsbk_byte, 0, sizeof(tsbk_byte));

  uint8_t r34byte_b[18];
  memset (r34byte_b, 0, sizeof(r34byte_b));

  //TODO: Expand storage to be able to hold 127 blocks + 1 header of data at 144 bits (18 octets
  uint8_t r34bytes[18*129]; //18 octets at 129 blocks (a little extra padding)
  memset (r34bytes, 0, sizeof(r34bytes));

  unsigned long long int PDU[18*129];
  memset (PDU, 0, 18*129*sizeof(unsigned long long int));

  int tsbk_decoded_bits[18*129*8]; //decoded bits from tsbk_bytes for sending to crc16_lb_bridge
  memset (tsbk_decoded_bits, 0, sizeof(tsbk_decoded_bits));

  uint8_t mpdu_decoded_bits[18*129*8];
  memset (mpdu_decoded_bits, 0, sizeof(mpdu_decoded_bits));

  uint8_t mpdu_crc_bits[18*129*8]; 
  memset (mpdu_crc_bits, 0, sizeof(mpdu_crc_bits));

  uint8_t mpdu_crc9_bits[18*129*8]; 
  memset (mpdu_crc9_bits, 0, sizeof(mpdu_crc9_bits));

  uint8_t mpdu_crc_bytes[18*129];
  memset (mpdu_crc_bytes, 0, sizeof(mpdu_crc_bytes));

  int i, j, k, x, z, l; z = 0; l = 0;
  int ec[129]; //error value returned from p25_12 or 34 trellis decoder
  int err[2]; //error value returned from crc16 on header and crc32 on full message
  memset (ec, -2, sizeof(ec));
  memset (err, -2, sizeof(err));

  int MFID = 0xFF; //Manufacturer ID

  uint8_t mpdu_byte[18*129];
  memset (mpdu_byte, 0, sizeof(mpdu_byte));

  uint8_t an = 0;
  uint8_t io = 0; 
  uint8_t fmt = 0;  
  uint8_t sap = 0; 
  uint8_t blks = 0; 
  uint8_t opcode = 0;

  uint32_t address = 0; //source, or destination address depending on the io bit (outbound is target)
  uint8_t fmf = 0;
  uint8_t pad = 0;
  uint8_t ns = 0;
  uint8_t fsnf = 0;
  uint8_t offset = 0;

  //response packet
  uint8_t class = 0;
  uint8_t type = 0;
  uint8_t status = 0;

  int end = 3; //ending value for data gathering repetitions (default at 3)

  //CRC32
  uint32_t CRCComputed = 0;
  uint32_t CRCExtracted = 0;

  int stop = 101;
  int start = 0;

  //now using modulus on skipdibit values
  int skipdibit = 36-14; //when we arrive here, we are at this position in the counter after reading FS, NID, DUID, and Parity dibits
  int status_count = 1; //we have already skipped the Status 1 dibit before arriving here
  int dibit_count = 0; //number of gathered dibits
  UNUSED(status_count); //debug counter

  //collect x-len reps of 100 or 101 dibits (98 valid dibits with two or three status dibits sprinkled in)
  for (j = 0; j < end; j++)
  {
    k = 0;
    dibit_count = 0;
    for (i = start; i < stop; i++)
    {
      dibit = getDibit(opts, state);
      if ( (skipdibit / 36) == 0)
      {
        dibit_count++;
        tsbk_dibit[k++] = dibit;
      }
      else
      {
        skipdibit = 0;
        status_count++;

        // fprintf (stderr, " S:%02d; D:%03d; i:%03d;", status_count, (j*101)+i+57, i); //debug Status Count, Total Dibit Count, and i (compare to symbol rx table)
        // fprintf (stderr, " S:%02d; CD:%02d; i:%03d; d:%d;", status_count, dibit_count, i, dibit); //debug Status Count, Current Dibit Count, i, and status dibit
        
        //debug, intepret status dibit (no fec or parity, so could easily be errroneous, could also be different on a data channel vs conventional?)
        // if (dibit == 0) fprintf (stderr, " Unknown - Use for Talk Around;");
        // if (dibit == 1) fprintf (stderr, " Inbound Channel Busy;");
        // if (dibit == 2) fprintf (stderr, " Unknown - Use for Inbound or Outbound;");
        // if (dibit == 3) fprintf (stderr, " Inbound Channel Idle;");

      }
      
      skipdibit++; //increment

      //this is used to skip gathering one dibit as well since we only will end up skipping 2 status dibits (getting 99 instead of 98, throwing alignment off)
      if (dibit_count == 98) //this could cause an issue though if the next bit read is supposed to be a status dibit (unsure) it may not matter, should be handled as first read in next rep
        break;
    }

    // fprintf (stderr, " DC: %d\n", dibit_count); //debug

    //send header to p25_12 and return tsbk_byte
    if (j == 0) ec[j] = p25_12 (tsbk_dibit, tsbk_byte);

    else if (r34)
    {
      //debug
      // fprintf (stderr, " J:%d;", j); //use this with the P_ERR inside of 34 rate decoder to see where the failures occur

      ec[j] = dmr_34 (tsbk_dibit, r34byte_b);

      //shuffle 34 rate data into array
      if (j != 0) //should never happen, but just in case
        memcpy (r34bytes+((j-1)*18), r34byte_b, sizeof(r34byte_b));

      for (i = 2; i < 18; i++)
      {
        for (x = 0; x < 8; x++)
          mpdu_crc_bits[z++] = ((r34byte_b[i] << x) & 0x80) >> 7;
      }

      //arrangement for confirmed data crc9 check
      //unlike DMR, the first 7 bits of this arrangement are the DBSN, not the last 7 bits
      for (x = 0; x < 7; x++)
        mpdu_crc9_bits[l++] = ((r34byte_b[0] << x) & 0x80) >> 7;
      for (i = 2; i < 18; i++)
      {
        for (x = 0; x < 8; x++)
          mpdu_crc9_bits[l++] = ((r34byte_b[i] << x) & 0x80) >> 7;
      }

    }
    else ec[j] = p25_12 (tsbk_dibit, tsbk_byte);

    //too many bit manipulations!
    k = 0;
    for (i = 0; i < 12; i++)
    {
      for (x = 0; x < 8; x++)
      {
        tsbk_decoded_bits[k] = ((tsbk_byte[i] << x) & 0x80) >> 7;
        k++;
      }
    }

    //CRC16 on the header
    if (j == 0) err[0] = crc16_lb_bridge(tsbk_decoded_bits, 80);

    //load into bit array for storage (easier decoding for future PDUs)
    for (i = 0; i < 96; i++) mpdu_decoded_bits[i+(j*96)] = (uint8_t)tsbk_decoded_bits[i];

    //shuffle corrected bits back into tsbk_byte
    k = 0;
    for (i = 0; i < 12; i++)
    {
      int byte = 0;
      for (x = 0; x < 8; x++)
      {
        byte = byte << 1;
        byte = byte | tsbk_decoded_bits[k];
        k++;
      }
      tsbk_byte[i] = byte;
      mpdu_byte[i+(j*12)] = byte; //add to completed MBF format 12 rate bytes
    }

    //check header data to see if this is a 12 rate, or 4 rate packet data unit
    if (j == 0 && err[0] == 0)
    {
      an   = (mpdu_byte[0] >> 6) & 0x1;
      io   = (mpdu_byte[0] >> 5) & 0x1;
      fmt  = mpdu_byte[0] & 0x1F;
      sap  = mpdu_byte[1] & 0x3F;
      blks = mpdu_byte[6] & 0x7F;
      MFID = mpdu_byte[2];
      address = (mpdu_byte[3] << 16) | (mpdu_byte[4] << 8) | mpdu_byte[5]; //LLID, inbound is the source, outbound is the target

      fmf = (mpdu_byte[6] >> 7) & 0x1;
      pad = mpdu_byte[7] & 0x1F;
      ns = (mpdu_byte[8] >> 4) & 0x7;
      fsnf = mpdu_byte[8] & 0xF;
      offset = mpdu_byte[9] & 0x3F;

      //response packet
      class  = (mpdu_byte[1] >> 6) & 0x3;
      type   = (mpdu_byte[1] >> 3) & 0x7;
      status = (mpdu_byte[1] >> 0) & 0x7;

      if (an == 1 && fmt == 0b10110) //confirmed data packet header block
        r34 = 1;

      //set end value to number of blocks + 1 header (block gathering for variable len)
      if (sap != 61 && sap != 63) //if not a trunking control block, this fixes an annoyance TSBK/TDULC blink in ncurses
        end = blks + 1;           //(TDU follows any Trunking MPDU, not sure if that's an error in handling, or actual behavior)

      //sanity check -- since blks is only 7 bit anyways, this probably isn't needed now
      if (end > 128) end = 128;  //Storage for up to 127 blocks plus 1 header

    }
    
  }

  if (err[0] == 0)
  {
    fprintf (stderr, "%s",KGRN);
    fprintf (stderr, " P25 Data - AN: %d; IO: %d; FMT: %02X; ", an, io, fmt);
    char sap_string[40];
    char rsp_string[40];
    if (fmt != 3) processSAP (sap, sap_string); //decode SAP to see what kind of data we are dealing with
    else          processRSP (class, type, status, rsp_string); //decode the response type (ack, nack, sack)
    if (sap != 61 && sap != 63) //Not too interested in viewing these on trunking control, just data packets mostly
      fprintf (stderr, "\n F: %d; Blocks: %02X; Pad: %d; NS: %d; FSNF: %d; Offset: %d; MFID: %02X;", fmf, blks, pad, ns, fsnf, offset, MFID);
    if (io == 1 && sap != 61 && sap != 63) //destination address if IO bit set
      fprintf (stderr, " Address: %d;", address);

    //Print to Data Call String for Ncurses Terminal (if not trunking control or response packet)
    if (sap != 61 && sap != 63 && fmt != 3)
      sprintf (state->dmr_lrrp_gps[0], "Data Call:%s SAP:%02X; Address: %d; ", sap_string, sap, address);

  } 
  else //crc error, so we can't validate information as accurate
  {
    fprintf (stderr, "%s",KRED);
    fprintf (stderr, " P25 Data Header CRC Error");
    fprintf (stderr, "%s",KNRM);
    end = 1; //go ahead and end after this loop
  }

  //trunking blocks
  if ((sap == 0x3D) && ((fmt == 0x17) || (fmt == 0x15)))
  {
    if (fmt == 0x15) fprintf (stderr, " UNC");
    else fprintf (stderr, " ALT");
    fprintf (stderr, " MBT");
    if (fmt == 0x17) opcode = mpdu_byte[7] & 0x3F; //alt MBT
    else opcode = mpdu_byte[12] & 0x3F; //unconf MBT
    fprintf (stderr, " - OP: %02X", opcode);


    //CRC32 is now working! 
    CRCExtracted = (mpdu_byte[(12*(blks+1))-4] << 24) | (mpdu_byte[(12*(blks+1))-3] << 16) | (mpdu_byte[(12*(blks+1))-2] << 8) | (mpdu_byte[(12*(blks+1))-1] << 0);
    CRCComputed  = crc32mbf(mpdu_byte+12, (96*blks)-32);
    if (CRCComputed == CRCExtracted) err[1] = 0;

    //group list mode so we can look and see if we need to block tuning any groups, etc
    char mode[8]; //allow, block, digital, enc, etc
    sprintf (mode, "%s", "");

    //if we are using allow/whitelist mode, then write 'B' to mode for block
    //comparison below will look for an 'A' to write to mode if it is allowed
    if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

    //just going to do a couple for now -- the extended format will not be vPDU compatible :(
    if (err[0] == 0 && err[1] == 0 && io == 1 && fmt == 0x17) //ALT Format
    {    
      //NET_STS_BCST -- TIA-102.AABC-D 6.2.11.2
      if (opcode == 0x3B)
      {
        int lra = mpdu_byte[3];
        int sysid = ((mpdu_byte[4] & 0xF) << 8) | mpdu_byte[5];
        int res_a = mpdu_byte[8];
        int res_b = mpdu_byte[9];
        long int wacn = (mpdu_byte[12] << 12) | (mpdu_byte[13] << 4) | (mpdu_byte[14] >> 4);
        int channelt = (mpdu_byte[15] << 8) | mpdu_byte[16];
        int channelr = (mpdu_byte[17] << 8) | mpdu_byte[18];
        int ssc =  mpdu_byte[19];
        UNUSED3(res_a, res_b, ssc);
        fprintf (stderr, "%s",KYEL);
        fprintf (stderr, "\n Network Status Broadcast MBT - Extended \n");
        fprintf (stderr, "  LRA [%02X] WACN [%05lX] SYSID [%03X] NAC [%03llX]\n", lra, wacn, sysid, state->p2_cc);
        fprintf (stderr, "  CHAN-T [%04X] CHAN-R [%04X]", channelt, channelr);
        long int ct_freq = process_channel_to_freq(opts, state, channelt);
        long int cr_freq = process_channel_to_freq(opts, state, channelr);
        UNUSED(cr_freq);
        
        state->p25_cc_freq = ct_freq;
        state->p25_cc_is_tdma = 0; //flag off for CC tuning purposes when system is qpsk

        //place the cc freq into the list at index 0 if 0 is empty, or not the same, 
        //so we can hunt for rotating CCs without user LCN list
        if (state->trunk_lcn_freq[0] == 0 || state->trunk_lcn_freq[0] != state->p25_cc_freq)
        {
          state->trunk_lcn_freq[0] = state->p25_cc_freq; 
        } 

        //only set IF these values aren't already hard set by the user
        if (state->p2_hardset == 0)
        {
          state->p2_wacn = wacn;
          state->p2_sysid = sysid;
        } 
      }
      //RFSS Status Broadcast - Extended 6.2.15.2
      else if (opcode == 0x3A) 
      {
        int lra = mpdu_byte[3];
        int lsysid = ((mpdu_byte[4] & 0xF) << 8) | mpdu_byte[5];
        int rfssid = mpdu_byte[12];
        int siteid = mpdu_byte[13];
        int channelt = (mpdu_byte[14] << 8) | mpdu_byte[15];
        int channelr = (mpdu_byte[16] << 8) | mpdu_byte[17];
        int sysclass = mpdu_byte[18];
        fprintf (stderr, "%s",KYEL);
        fprintf (stderr, "\n RFSS Status Broadcast MBF - Extended \n");
        fprintf (stderr, "  LRA [%02X] SYSID [%03X] RFSS ID [%03d] SITE ID [%03d]\n  CHAN-T [%04X] CHAN-R [%02X] SSC [%02X] ", lra, lsysid, rfssid, siteid, channelt, channelr, sysclass);
        process_channel_to_freq (opts, state, channelt);
        process_channel_to_freq (opts, state, channelr);

        state->p2_siteid = siteid;
        state->p2_rfssid = rfssid;
      }

      //Adjacent Status Broadcast (ADJ_STS_BCST) Extended 6.2.2.2
      else if (opcode == 0x3C) 
      {
        int lra = mpdu_byte[3];
        int cfva = mpdu_byte[4] >> 4;
        int lsysid = ((mpdu_byte[4] & 0xF) << 8) | mpdu_byte[5];
        int rfssid = mpdu_byte[8];
        int siteid = mpdu_byte[9];
        int channelt = (mpdu_byte[12] << 8) | mpdu_byte[13];
        int channelr = (mpdu_byte[14] << 8) | mpdu_byte[15];
        int sysclass = mpdu_byte[16];  
        long int wacn = (mpdu_byte[17] << 12) | (mpdu_byte[18] << 4) | (mpdu_byte[19] >> 4);
        fprintf (stderr, "%s",KYEL);
        fprintf (stderr, "\n Adjacent Status Broadcast - Extended\n");
        fprintf (stderr, "  LRA [%02X] CFVA [%X] RFSS[%03d] SITE [%03d] SYSID [%03X]\n  CHAN-T [%04X] CHAN-R [%04X] SSC [%02X] WACN [%05lX]\n  ", lra, cfva, rfssid, siteid, lsysid, channelt, channelr, sysclass, wacn);
        if (cfva & 0x8) fprintf (stderr, " Conventional");
        if (cfva & 0x4) fprintf (stderr, " Failure Condition");
        if (cfva & 0x2) fprintf (stderr, " Up to Date (Correct)");
        else fprintf (stderr, " Last Known");
        if (cfva & 0x1) fprintf (stderr, " Valid RFSS Connection Active");
        process_channel_to_freq (opts, state, channelt);
        process_channel_to_freq (opts, state, channelr);

      }

      //Group Voice Channel Grant - Extended 
      else if (opcode == 0x0) 
      {
        int svc = mpdu_byte[8];
        int channelt  = (mpdu_byte[14] << 8) | mpdu_byte[15];
        int channelr  = (mpdu_byte[16] << 8) | mpdu_byte[17];
        long int source = (mpdu_byte[3] << 16) |(mpdu_byte[4] << 8) | mpdu_byte[5];
        int group = (mpdu_byte[18] << 8) | mpdu_byte[19];
        long int freq1 = 0;
        long int freq2 = 0;
        UNUSED2(source, freq2);
        fprintf (stderr, "%s\n ",KYEL);
        if (svc & 0x80) fprintf (stderr, " Emergency");
        if (svc & 0x40) fprintf (stderr, " Encrypted");

        if (opts->payload == 1) //hide behind payload due to len
        {
          if (svc & 0x20) fprintf (stderr, " Duplex");
          if (svc & 0x10) fprintf (stderr, " Packet");
          else fprintf (stderr, " Circuit");
          if (svc & 0x8) fprintf (stderr, " R"); //reserved bit is on
          fprintf (stderr, " Priority %d", svc & 0x7); //call priority
        }
        fprintf (stderr, " Group Voice Channel Grant Update - Extended");
        fprintf (stderr, "\n  SVC [%02X] CHAN-T [%04X] CHAN-R [%04X] Group [%d][%04X]", svc, channelt, channelr, group, group);
        freq1 = process_channel_to_freq (opts, state, channelt);
        freq2 = process_channel_to_freq (opts, state, channelr);

        //add active channel to string for ncurses display
        sprintf (state->active_channel[0], "Active Ch: %04X TG: %d ", channelt, group);
        state->last_active_time = time(NULL);

        for (int i = 0; i < state->group_tally; i++)
        {
          if (state->group_array[i].groupNumber == group)
          {
            fprintf (stderr, " [%s]", state->group_array[i].groupName);
            strcpy (mode, state->group_array[i].groupMode);
            break;
          }
        }

        //TG hold on P25p1 Ext -- block non-matching target, allow matching group
        if (state->tg_hold != 0 && state->tg_hold != group) sprintf (mode, "%s", "B");
        if (state->tg_hold != 0 && state->tg_hold == group) sprintf (mode, "%s", "A");
        
        //Skip tuning group calls if group calls are disabled
        if (opts->trunk_tune_group_calls == 0) goto SKIPCALL;

        //Skip tuning encrypted calls if enc calls are disabled
        if ( (svc & 0x40) && opts->trunk_tune_enc_calls == 0) goto SKIPCALL;

        //tune if tuning available
        if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0))
        {
          //reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
          if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && freq1 != 0) //if we aren't already on a VC and have a valid frequency
          {

            //changed to allow symbol rate change on C4FM Phase 2 systems as well as QPSK
            if (1 == 1)
            {
              if (state->p25_chan_tdma[channelt >> 12] == 1)
              {
                state->samplesPerSymbol = 8;
                state->symbolCenter = 3;

                //shim fix to stutter/lag by only enabling slot on the target/channel we tuned to
                //this will only occur in realtime tuning, not not required .bin or .wav playback
                if (channelt & 1) //VCH1
                {
                  opts->slot1_on = 0;
                  opts->slot2_on = 1;
                }
                else //VCH0
                {
                  opts->slot1_on = 1;
                  opts->slot2_on = 0;
                }

              }
            }
            //rigctl
            if (opts->use_rigctl == 1)
            {
              if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
              SetFreq(opts->rigctl_sockfd, freq1);
              state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq1;
              opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop 
              state->last_vc_sync_time = time(NULL);
            }
            //rtl
            else if (opts->audio_in_type == 3)
            {
              #ifdef USE_RTLSDR
              rtl_dev_tune (opts, freq1);
              state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq1;
              opts->p25_is_tuned = 1;
              state->last_vc_sync_time = time(NULL);
              #endif
            }
          }    
        }
      }

      //Unit to Unit Voice Channel Grant - Extended
      else if (opcode == 0x6) 
      {
        //I'm not doing EVERY element of this, just enough for tuning!
        int svc = mpdu_byte[8];
        int channelt  = (mpdu_byte[22] << 8) | mpdu_byte[23];
        int channelr  = (mpdu_byte[24] << 8) | mpdu_byte[25]; //optional!
        //using source and target address, not source and target id (is this correct?)
        long int source = (mpdu_byte[3] << 16)  | (mpdu_byte[4] << 8) | mpdu_byte[5];
        long int target = (mpdu_byte[19] << 16) |(mpdu_byte[20] << 8) | mpdu_byte[21];
        //TODO: Test Full Values added here for SUID, particular tgt_nid and tgt_sid
        long int src_nid = (mpdu_byte[12] << 24) | (mpdu_byte[13] << 16) | (mpdu_byte[14] << 8) | mpdu_byte[15];
        long int src_sid = (mpdu_byte[16] << 16) | (mpdu_byte[17] << 8) | mpdu_byte[18];
        long int tgt_nid = (mpdu_byte[26] << 16) | (mpdu_byte[27] << 8) | mpdu_byte[28]; //only has 3 octets on tgt nid, partial only?
        long int tgt_sid = (mpdu_byte[29] << 16) | (mpdu_byte[30] << 8) | mpdu_byte[31];
        long int freq1 = 0;
        long int freq2 = 0;
        UNUSED(freq2);
        fprintf (stderr, "%s\n ",KYEL);
        if (svc & 0x80) fprintf (stderr, " Emergency");
        if (svc & 0x40) fprintf (stderr, " Encrypted");

        if (opts->payload == 1) //hide behind payload due to len
        {
          if (svc & 0x20) fprintf (stderr, " Duplex");
          if (svc & 0x10) fprintf (stderr, " Packet");
          else fprintf (stderr, " Circuit");
          if (svc & 0x8) fprintf (stderr, " R"); //reserved bit is on
          fprintf (stderr, " Priority %d", svc & 0x7); //call priority
        }
        fprintf (stderr, " Unit to Unit Voice Channel Grant Update - Extended");
        fprintf (stderr, "\n  SVC: %02X; CHAN-T: %04X; CHAN-R: %04X; SRC: %ld; TGT: %ld; FULL SRC: %08lX-%08ld; FULL TGT: %08lX-%08ld;", svc, channelt, channelr, source, target, src_nid, src_sid, tgt_nid, tgt_sid);
        freq1 = process_channel_to_freq (opts, state, channelt);
        freq2 = process_channel_to_freq (opts, state, channelr); //optional!

        //add active channel to string for ncurses display
        sprintf (state->active_channel[0], "Active Ch: %04X TGT: %ld; ", channelt, target);

        for (int i = 0; i < state->group_tally; i++)
        {
          if (state->group_array[i].groupNumber == target)
          {
            fprintf (stderr, " [%s]", state->group_array[i].groupName);
            strcpy (mode, state->group_array[i].groupMode);
            break;
          }
        }

        //TG hold on P25p1 Ext UU -- will want to disable UU_V grants while TG Hold enabled
        if (state->tg_hold != 0 && state->tg_hold != target) sprintf (mode, "%s", "B");
        // if (state->tg_hold != 0 && state->tg_hold == target) sprintf (mode, "%s", "A");
        
        //Skip tuning private calls if group calls are disabled
        if (opts->trunk_tune_private_calls == 0) goto SKIPCALL;

        //Skip tuning encrypted calls if enc calls are disabled
        if ( (svc & 0x40) && opts->trunk_tune_enc_calls == 0) goto SKIPCALL;

        //tune if tuning available
        if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0))
        {
          //reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
          if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && freq1 != 0) //if we aren't already on a VC and have a valid frequency
          {

            //changed to allow symbol rate change on C4FM Phase 2 systems as well as QPSK
            if (1 == 1)
            {
              if (state->p25_chan_tdma[channelt >> 12] == 1)
              {
                state->samplesPerSymbol = 8;
                state->symbolCenter = 3;

                //shim fix to stutter/lag by only enabling slot on the target/channel we tuned to
                //this will only occur in realtime tuning, not not required .bin or .wav playback
                if (channelt & 1) //VCH1
                {
                  opts->slot1_on = 0;
                  opts->slot2_on = 1;
                }
                else //VCH0
                {
                  opts->slot1_on = 1;
                  opts->slot2_on = 0;
                }

              }
            }
            //rigctl
            if (opts->use_rigctl == 1)
            {
              if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
              SetFreq(opts->rigctl_sockfd, freq1);
              state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq1;
              opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop
              state->last_vc_sync_time = time(NULL);
            }
            //rtl
            else if (opts->audio_in_type == 3)
            {
              #ifdef USE_RTLSDR
              rtl_dev_tune (opts, freq1);
              state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq1;
              opts->p25_is_tuned = 1;
              state->last_vc_sync_time = time(NULL);
              #endif
            }
          }    
        }
      }

      //Telephone Interconnect Voice Channel Grant (or Update) -- Explicit Channel Form
      else if ( (opcode == 0x8 || opcode == 0x9) && MFID < 2) //This form does allow for other MFID's but Moto has a seperate function on 9
      {
        //TELE_INT_CH_GRANT or TELE_INT_CH_GRANT_UPDT
        int svc = mpdu_byte[8];
        int channel = (mpdu_byte[12] << 8) | mpdu_byte[13];
        int timer   = (mpdu_byte[16] << 8) | mpdu_byte[17];
        int target  = (mpdu_byte[3] << 16) | (mpdu_byte[4] << 8) | mpdu_byte[5];
        long int freq = 0;
        fprintf (stderr, "\n");
        if (svc & 0x80) fprintf (stderr, " Emergency");
        if (svc & 0x40) fprintf (stderr, " Encrypted");

        if (opts->payload == 1) //hide behind payload due to len
        {
          if (svc & 0x20) fprintf (stderr, " Duplex");
          if (svc & 0x10) fprintf (stderr, " Packet");
          else fprintf (stderr, " Circuit");
          if (svc & 0x8) fprintf (stderr, " R"); //reserved bit is on
          fprintf (stderr, " Priority %d", svc & 0x7); //call priority
        }

        fprintf (stderr, " Telephone Interconnect Voice Channel Grant");
        if ( opcode & 1) fprintf (stderr, " Update");
        fprintf (stderr, " Extended");
        fprintf (stderr, "\n  CHAN: %04X; Timer: %f Seconds; Target: %d;", channel, (float)timer*0.1f, target); //timer unit is 100 ms, or 0.1 seconds
        freq = process_channel_to_freq (opts, state, channel);

        //add active channel to string for ncurses display
        sprintf (state->active_channel[0], "Active Tele Ch: %04X TGT: %d; ", channel, target);
        state->last_active_time = time(NULL);

        //Skip tuning private calls if private calls is disabled (are telephone int calls private, or talkgroup?)
        if (opts->trunk_tune_private_calls == 0) goto SKIPCALL; 

        //Skip tuning encrypted calls if enc calls are disabled
        if ( (svc & 0x40) && opts->trunk_tune_enc_calls == 0) goto SKIPCALL;

        //telephone only has a target address (manual shows combined source/target of 24-bits)
        for (int i = 0; i < state->group_tally; i++)
        {
          if (state->group_array[i].groupNumber == target)
          {
            fprintf (stderr, " [%s]", state->group_array[i].groupName);
            strcpy (mode, state->group_array[i].groupMode);
            break;
          }
        }

        //TG hold on UU_V -- will want to disable UU_V grants while TG Hold enabled
        if (state->tg_hold != 0 && state->tg_hold != target) sprintf (mode, "%s", "B");

        //tune if tuning available
        if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0))
        {
          //reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
          if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && freq != 0) //if we aren't already on a VC and have a valid frequency
          {
            //changed to allow symbol rate change on C4FM Phase 2 systems as well as QPSK
            if (1 == 1)
            {
              if (state->p25_chan_tdma[channel >> 12] == 1)
              {
                state->samplesPerSymbol = 8;
                state->symbolCenter = 3;

                //shim fix to stutter/lag by only enabling slot on the target/channel we tuned to
                //this will only occur in realtime tuning, not not required .bin or .wav playback
                if (channel & 1) //VCH1
                {
                  opts->slot1_on = 0;
                  opts->slot2_on = 1;
                }
                else //VCH0
                {
                  opts->slot1_on = 1;
                  opts->slot2_on = 0;
                }
              }

            }

            //rigctl
            if (opts->use_rigctl == 1)
            {
              if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
              SetFreq(opts->rigctl_sockfd, freq);
              if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = freq;
              opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop
              state->last_vc_sync_time = time(NULL); 
            }
            //rtl
            else if (opts->audio_in_type == 3)
            {
              #ifdef USE_RTLSDR
              rtl_dev_tune (opts, freq);
              if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = freq;
              opts->p25_is_tuned = 1;
              state->last_vc_sync_time = time(NULL);
              #endif
            }
          }    
        }
        if (opts->p25_trunk == 0)
        {
          if (target == state->lasttg || target == state->lasttgR)
          {
            //P1 FDMA
            if (state->synctype == 0 || state->synctype == 1) state->p25_vc_freq[0] = freq;
            //P2 TDMA
            else state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq;
          }
        }

      }

      //look at Harris Opcodes and payload portion of MPDU
      else if (MFID == 0xA4) 
      {
        //TODO: Add Known Opcodes from Manual (all one of them)
        fprintf (stderr, "%s",KCYN);
        fprintf (stderr, "\n MFID A4 (Harris); Opcode: %02X; ", opcode);
        for (i = 0; i < (12*(blks+1)%37); i++)
          fprintf (stderr, "%02X", mpdu_byte[i]);
        fprintf (stderr, " %s",KNRM);
      }

      //look at Motorola Opcodes and payload portion of MPDU
      else if (MFID == 0x90)
      {
        //TIA-102.AABH
        if (opcode == 0x02)
        {
          int svc = mpdu_byte[8]; //Just the Res, P-bit, and more res bits
          int channelt  = (mpdu_byte[12] << 8) | mpdu_byte[13];
          int channelr  = (mpdu_byte[14] << 8) | mpdu_byte[15];
          long int source = (mpdu_byte[3] << 16) |(mpdu_byte[4] << 8) | mpdu_byte[5];
          int group = (mpdu_byte[16] << 8) | mpdu_byte[17];
          long int freq1 = 0;
          long int freq2 = 0;
          UNUSED2(source, freq2);
          fprintf (stderr, "%s\n ",KYEL);

          if (svc & 0x40) fprintf (stderr, " Encrypted"); //P-bit

          fprintf (stderr, " MFID90 Group Regroup Channel Grant - Explicit");
          fprintf (stderr, "\n  RES/P [%02X] CHAN-T [%04X] CHAN-R [%04X] SG [%d][%04X]", svc, channelt, channelr, group, group);
          freq1 = process_channel_to_freq (opts, state, channelt);
          freq2 = process_channel_to_freq (opts, state, channelr);

          //add active channel to string for ncurses display
          sprintf (state->active_channel[0], "MFID90 Ch: %04X SG: %d ", channelt, group);
          state->last_active_time = time(NULL);

          for (int i = 0; i < state->group_tally; i++)
          {
            if (state->group_array[i].groupNumber == group)
            {
              fprintf (stderr, " [%s]", state->group_array[i].groupName);
              strcpy (mode, state->group_array[i].groupMode);
              break;
            }
          }

          //TG hold on MFID90 GRG -- block non-matching target, allow matching group
          if (state->tg_hold != 0 && state->tg_hold != group) sprintf (mode, "%s", "B");
          if (state->tg_hold != 0 && state->tg_hold == group) sprintf (mode, "%s", "A");
          
          //Skip tuning group calls if group calls are disabled
          if (opts->trunk_tune_group_calls == 0) goto SKIPCALL;

          //Skip tuning encrypted calls if enc calls are disabled
          if ( (svc & 0x40) && opts->trunk_tune_enc_calls == 0) goto SKIPCALL;

          //tune if tuning available
          if (opts->p25_trunk == 1 && (strcmp(mode, "DE") != 0) && (strcmp(mode, "B") != 0))
          {
            //reworked to set freq once on any call to process_channel_to_freq, and tune on that, independent of slot
            if (state->p25_cc_freq != 0 && opts->p25_is_tuned == 0 && freq1 != 0) //if we aren't already on a VC and have a valid frequency
            {

              //changed to allow symbol rate change on C4FM Phase 2 systems as well as QPSK
              if (1 == 1)
              {
                if (state->p25_chan_tdma[channelt >> 12] == 1)
                {
                  state->samplesPerSymbol = 8;
                  state->symbolCenter = 3;

                  //shim fix to stutter/lag by only enabling slot on the target/channel we tuned to
                  //this will only occur in realtime tuning, not not required .bin or .wav playback
                  if (channelt & 1) //VCH1
                  {
                    opts->slot1_on = 0;
                    opts->slot2_on = 1;
                  }
                  else //VCH0
                  {
                    opts->slot1_on = 1;
                    opts->slot2_on = 0;
                  }

                }
              }
              //rigctl
              if (opts->use_rigctl == 1)
              {
                if (opts->setmod_bw != 0 ) SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
                SetFreq(opts->rigctl_sockfd, freq1);
                state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq1;
                opts->p25_is_tuned = 1; //set to 1 to set as currently tuned so we don't keep tuning nonstop 
                state->last_vc_sync_time = time(NULL);
              }
              //rtl
              else if (opts->audio_in_type == 3)
              {
                #ifdef USE_RTLSDR
                rtl_dev_tune (opts, freq1);
                state->p25_vc_freq[0] = state->p25_vc_freq[1] = freq1;
                opts->p25_is_tuned = 1;
                state->last_vc_sync_time = time(NULL);
                #endif
              }
            }    
          }
        }

        else
        {
          fprintf (stderr, "%s",KCYN);
          fprintf (stderr, "\n MFID 90 (Moto); Opcode: %02X; ", mpdu_byte[0] & 0x3F);
          for (i = 0; i < (12*(blks+1)%37); i++)
            fprintf (stderr, "%02X", mpdu_byte[i]);
          fprintf (stderr, " %s",KNRM);
        }
      }

      else
      {
        fprintf (stderr, "%s",KCYN);
        fprintf (stderr, "\n MFID %02X (Unknown); Opcode: %02X; ", MFID, mpdu_byte[0] & 0x3F);
        for (i = 0; i < (12*(blks+1)%37); i++)
          fprintf (stderr, "%02X", mpdu_byte[i]);
        fprintf (stderr, " %s",KNRM);
      }
    }

    SKIPCALL: ; //do nothing

    if (opts->payload == 1)
    {
      fprintf (stderr, "%s",KCYN);
      fprintf (stderr, "\n P25 MBT Payload \n  ");
      for (i = 0; i < ((blks+1)*12); i++)
      {
        if ( (i != 0) && ((i % 12) == 0))
          fprintf (stderr, "\n  ");
        fprintf (stderr, "[%02X]", mpdu_byte[i]);

      }

      fprintf (stderr, "\n "); 
      fprintf (stderr, " CRC EXT %08X CMP %08X", CRCExtracted, CRCComputed);
      fprintf (stderr, "%s ", KNRM);

      //Header 
      if (err[0] != 0) 
      {
        fprintf (stderr, "%s",KRED);
        fprintf (stderr, " (HDR CRC16 ERR)");
        fprintf (stderr, "%s",KCYN);
      }
      //Completed MBF 
      if (err[1] != 0) 
      {
        fprintf (stderr, "%s",KRED);
        fprintf (stderr, " (MBF CRC32 ERR)");
        fprintf (stderr, "%s",KCYN);
      }

    }

    fprintf (stderr, "%s ", KNRM);
    fprintf (stderr, "\n");

  } //end trunking block format

  else if (r34 == 1 && err[0] == 0) //start 34 rate dump if good header crc
  {

    //ES Auxiliary Header
    if (sap == 1)
    {
      fprintf (stderr, "%s",KYEL);
      unsigned long long int mi = (unsigned long long int)ConvertBitIntoBytes(&mpdu_crc_bits[0], 64);
      uint8_t  mi_res = (uint8_t)ConvertBitIntoBytes(&mpdu_crc_bits[64], 8);
      uint8_t  alg_id = (uint8_t)ConvertBitIntoBytes(&mpdu_crc_bits[72], 8);
      uint16_t key_id = (uint16_t)ConvertBitIntoBytes(&mpdu_crc_bits[80], 16);
      fprintf (stderr, "\n ALG: %02X; KEY ID: %04X; MI: %016llX; ", alg_id, key_id, mi);
      if (mi_res != 0)
        fprintf (stderr, "RES: %02X;", mi_res);

      //The Auxiliary Header signals the actual SAP value of the encrypted message (this byte is not encrypted)
      uint8_t aux_res = (uint8_t)ConvertBitIntoBytes(&mpdu_crc_bits[96], 2); //these two bits should always be signalled as 1's, so 0b11, and if combined with the 2ndary SAP, 0xC0 if SAP == 0x00
      uint8_t aux_sap = (uint8_t)ConvertBitIntoBytes(&mpdu_crc_bits[98], 6); //the SAP of the message that is encrypted immediately after
      char aux_sap_string[99];
      processSAP (aux_sap, aux_sap_string);
      fprintf (stderr, "%s",KNRM);
      UNUSED(aux_res);
    }

    // if (opts->payload == 1) //always dump these for now since users are actively choosing to tune to these grants
    {
      fprintf (stderr, "%s",KCYN);
      fprintf (stderr, "\n P25 MPDU 34 Rate Payload \n  ");
      for (i = 0; i < 12; i++) //header
        fprintf (stderr, "%02X", mpdu_byte[i]);
      fprintf (stderr, " Header \n  ");

      memset (mpdu_byte, 0, sizeof(mpdu_byte));
      for (i = 0; i < 16*(blks+1); i++) //16*10
        mpdu_byte[i] = (uint8_t)ConvertBitIntoBytes(&mpdu_crc_bits[i*8], 8);

      CRCExtracted = (uint32_t)ConvertBitIntoBytes(&mpdu_crc_bits[(128*blks)-32], 32);
      CRCComputed  = crc32mbf(mpdu_byte, (128*blks)-32);
      if (CRCComputed == CRCExtracted) err[1] = 0;

      //reset mpdu_byte to load only the data, and not the dbsn and crc into
      memset (mpdu_byte, 0, sizeof(mpdu_byte));
      int mpdu_idx = 0;
      int next = 0;
      //variable len printing
      for (i = 2; i <= 18*blks; i++) //<= only because we want that final DBSN/CRC9 printed out
      {
        if ( (i != 0) && ((i % 18) == 0))
        {
          uint8_t dbsn = r34bytes[i-18] >> 1; //get the previous DBSN at this point
          uint16_t crc9_ext = ((r34bytes[i-18] & 1) << 8) | r34bytes[i-17];
          uint16_t crc9_cmp = ComputeCrc9Bit(mpdu_crc9_bits+next, 135);
          next += 135;
          if (crc9_ext == crc9_cmp)
            fprintf (stderr, " DBSN: %d;", dbsn+1);
          else
          {
            fprintf (stderr, "%s",KRED);
            fprintf (stderr, " CRC ERR;");
            fprintf (stderr, "%s",KCYN);
            // fprintf (stderr, " EXT: %03X; CMP: %03X", crc9_ext, crc9_cmp);
          }
          if (i != 18*blks) i += 2; //skip the next DBSN/CRC9
          if (i != 18*blks) fprintf (stderr, "\n  ");
        }
        if (i != 18*blks) fprintf (stderr, "%02X", r34bytes[i]);
        mpdu_byte[mpdu_idx++] = r34bytes[i];
      }

      if (err[1] != 0) 
      {
        fprintf (stderr, "%s",KRED);
        fprintf (stderr, "\n  (MPDU CRC32 ERR)");
        fprintf (stderr, "%s",KCYN);
        fprintf (stderr, " CRC EXT %08X CMP %08X", CRCExtracted, CRCComputed);
      }
      // else
      //   TODO: decode PDU function

      #define P25_ASCII_TEST34
      #ifdef P25_ASCII_TEST34
      // if (sap == 0) //user data
      {
        fprintf (stderr, "%s",KCYN);
        fprintf (stderr, "\n P25 MPDU ASCII: ");
        start = 0; //offset for any sort of aux headers
        if (sap == 1) start = 13;
        for (i = start; i < mpdu_idx-pad-4; i++)
        {
          if (mpdu_byte[i] <= 0x7E && mpdu_byte[i] >=0x20)
            fprintf (stderr, "%c", mpdu_byte[i]);
          else fprintf (stderr, " ");
        }
      }
      #endif

      // #define P25_UTF16_TEST
      #ifdef P25_UTF16_TEST
      //utf-16 text messaging (debug)
      // if (utf-16)
      {
        fprintf (stderr, "%s", KCYN);
        fprintf (stderr, "\n P25 MPDU UTF16: ");
        uint16_t ch16 = 0;
        start = 0; //offset for any sort of aux headers
        if (sap == 1) start = 13;
        for (i = start; i < mpdu_idx-pad-4;)
        {
          ch16 = (uint16_t)mpdu_byte[i+0];
          ch16 <<= 8;
          ch16 |= (uint16_t)mpdu_byte[i+1];
          // fprintf (stderr, " %04X; ", ch16); //debug for raw values to check grouping for offset

          if (ch16 >= 0x20) //if not a linebreak or terminal commmands
            fprintf (stderr, "%lc", ch16);
          else if (ch16 == 0) //if padding
            fprintf (stderr, "_");
          else fprintf (stderr, " ");

          i += 2;
        }
      }
      #endif

      fprintf (stderr, "%s ", KNRM);
      fprintf (stderr, "\n");      

    }
  } //end r34
  else if (err[0] == 0) //12 rate unconfirmed data
  {

    if (blks != 0)
    {
      CRCExtracted = (mpdu_byte[(12*(blks+1))-4] << 24) | (mpdu_byte[(12*(blks+1))-3] << 16) | (mpdu_byte[(12*(blks+1))-2] << 8) | (mpdu_byte[(12*(blks+1))-1] << 0);
      CRCComputed  = crc32mbf(mpdu_byte+12, (96*blks)-32);
      if (CRCComputed == CRCExtracted) err[1] = 0;
    }
    else err[1] = 0; //No CRC32 on a lonely header

    //ES Auxiliary Header (need to find samples to confirm presence and correctness on 1/2 rate data)
    if (sap == 1 && blks != 0) //may not need blocks check, since if this is present, multiple blocks will be required
    {
      fprintf (stderr, "%s",KYEL);
      unsigned long long int mi = ((unsigned long long int)mpdu_byte[12] << 56ULL) | ((unsigned long long int)mpdu_byte[13] << 48ULL) | ((unsigned long long int)mpdu_byte[14] << 40ULL) | ((unsigned long long int)mpdu_byte[15] << 32ULL) |
                                  ((unsigned long long int)mpdu_byte[16] << 24ULL) | ((unsigned long long int)mpdu_byte[17] << 16ULL) | ((unsigned long long int)mpdu_byte[18] << 8ULL)  | ((unsigned long long int)mpdu_byte[19] << 0ULL);
      uint8_t  mi_res = mpdu_byte[20];
      uint8_t  alg_id = mpdu_byte[21];
      uint16_t key_id = (mpdu_byte[22] << 8) | mpdu_byte[23];
      fprintf (stderr, "\n ALG: %02X; KEY ID: %04X; MI: %016llX; ", alg_id, key_id, mi);
      if (mi_res != 0)
        fprintf (stderr, "RES: %02X;", mi_res);

      //The Auxiliary Header signals the actual SAP value of the encrypted message (this byte is not encrypted)
      uint8_t aux_res = mpdu_byte[24] >> 6; //these two bits should always be signalled as 1's, so 0b11, and if combined with the 2ndary SAP, 0xC0 if SAP == 0x00
      uint8_t aux_sap = mpdu_byte[24] & 0x3F; //the SAP of the message that is encrypted immediately after
      char aux_sap_string[99];
      processSAP (aux_sap, aux_sap_string);
      fprintf (stderr, "%s",KNRM);
      UNUSED(aux_res);
    }

    // if (opts->payload == 1)
    {
      fprintf (stderr, "%s",KCYN);
      fprintf (stderr, "\n P25 MPDU 12 Rate Payload: \n  ");
      for (i = 0; i < 12*(blks+1); i++) //header and payload combined
      {
        if (i == 12) fprintf (stderr, " Header");
        if ( (i != 0) && ((i % 12) == 0))
          fprintf (stderr, "\n  ");
        fprintf (stderr, "%02X", mpdu_byte[i]);
      }
    }

    if (err[1] != 0) 
    {
      fprintf (stderr, "%s",KRED);
      fprintf (stderr, "\n  (MPDU CRC32 ERR)");
      fprintf (stderr, "%s",KCYN);
      fprintf (stderr, " CRC EXT %08X CMP %08X", CRCExtracted, CRCComputed);
    }
    // else if (blks != 0)
    //     TODO: decode PDU function

    #define P25_ASCII_TEST12
    #ifdef P25_ASCII_TEST12
    // if (opts->payload == 1)
    {
      fprintf (stderr, "%s",KCYN);
      fprintf (stderr, "\n P25 MPDU ASCII: ");
      start = 0; //offset for any sort of aux headers
      if (sap == 1) start = 13;
      for (i = start; i < (12*(blks+1))-4-pad; i++)
      {
        if (mpdu_byte[i] <= 0x7E && mpdu_byte[i] >=0x20)
          fprintf (stderr, "%c", mpdu_byte[i]);
        else fprintf (stderr, " ");
      }
    }
    #endif

    // #define P25_UTF16_TEST12
    #ifdef P25_UTF16_TEST12
    //utf-16 text messaging (debug)
    // if (utf-16)
    {
      fprintf (stderr, "%s", KCYN);
      fprintf (stderr, "\n P25 MPDU UTF16: ");
      uint16_t ch16 = 0;
      start = 0; //offset for any sort of aux headers
      if (sap == 1) start = 13;
      for (i = start; i < (12*blks)-4-pad;)
      {
        ch16 = (uint16_t)mpdu_byte[i+0];
        ch16 <<= 8;
        ch16 |= (uint16_t)mpdu_byte[i+1];
        // fprintf (stderr, " %04X; ", ch16); //debug for raw values to check grouping for offset

        if (ch16 >= 0x20) //if not a linebreak or terminal commmands
          fprintf (stderr, "%lc", ch16);
        else if (ch16 == 0) //if padding
          fprintf (stderr, "_");
        else fprintf (stderr, " ");

        i += 2;
      }
    }
    #endif

    fprintf (stderr, "%s",KNRM);
    fprintf (stderr, "\n");

  }
  else //crc header failure or other
  {
    fprintf (stderr, "%s",KNRM);
    fprintf (stderr, "\n");
  }
  
}
