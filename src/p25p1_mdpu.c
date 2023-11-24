/*-------------------------------------------------------------------------------
 * p25p1_mpdu.c
 * P25p1 Multi Block PDU and Multi Block Trunking
 *
 * LWVMOBILE
 * 2023-03 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"

void processSAP(uint8_t SAP)
{

  if      (SAP == 0)  fprintf (stderr, " User Data;");
  else if (SAP == 1)  fprintf (stderr, " Encrypted User Data;");
  else if (SAP == 2)  fprintf (stderr, " Circuit Data;");
  else if (SAP == 3)  fprintf (stderr, " Circuit Data Control;");
  else if (SAP == 4)  fprintf (stderr, " Packet Data;");
  else if (SAP == 5)  fprintf (stderr, " Address Resolution Protocol;");
  else if (SAP == 6)  fprintf (stderr, " SNDCP Packet Data Control;");
  else if (SAP == 31) fprintf (stderr, " Extended Address;");
  else if (SAP == 32) fprintf (stderr, " Registration and Authorization;");
  else if (SAP == 33) fprintf (stderr, " Channel Reassignment;");
  else if (SAP == 34) fprintf (stderr, " System Configuration;");
  else if (SAP == 35) fprintf (stderr, " Mobile Radio Loopback;");
  else if (SAP == 36) fprintf (stderr, " Mobile Radio Statistics;");
  else if (SAP == 37) fprintf (stderr, " Mobile Radio Out of Service;");
  else if (SAP == 38) fprintf (stderr, " Mobile Radio Paging;");
  else if (SAP == 39) fprintf (stderr, " Mobile Radio Configuration;");
  else if (SAP == 40) fprintf (stderr, " Unencrypted Key Management;");
  else if (SAP == 41) fprintf (stderr, " Encrypted Key Management;");
  else if (SAP == 61) fprintf (stderr, " Trunking Control;");
  else if (SAP == 63) fprintf (stderr, " Encrypted Trunking Control;");

  //speculative values below based on observation and guess work

  //MPDUs immediately following this seems to be encrypted based on the padding octets being non-zeroes (or could just be a decoder failure)
  else if (SAP == 8)  fprintf (stderr, " Encrytion Parameters?;");
  //observed values, but no idea, they all seem to have the same octet values (address of some sort?)
  else if (SAP == 0x9)   fprintf (stderr, " Unknown SAP;");
  else if (SAP == 0xA)   fprintf (stderr, " Unknown SAP;");
  else if (SAP == 0xB)   fprintf (stderr, " Unknown SAP;");
  else if (SAP == 0xC)   fprintf (stderr, " Unknown SAP;");
  else if (SAP == 0xE)   fprintf (stderr, " Unknown SAP;");
  else if (SAP == 0x2E)  fprintf (stderr, " Unknown SAP;");
  
  //catch all for everything else
  else                fprintf (stderr, " Unknown SAP;");

}

static uint32_t crc32mbf(uint8_t buf[], int len)
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

  //reset some strings when returning from a call in case they didn't get zipped already
  sprintf (state->call_string[0], "%s", "                     "); //21 spaces
  sprintf (state->call_string[1], "%s", "                     "); //21 spaces

  //clear stale Active Channel messages here
  if ( (time(NULL) - state->last_active_time) > 3 )
  {
    memset (state->active_channel, 0, sizeof(state->active_channel));
  }

  int tsbkbit[196]; //tsbk bit array, 196 trellis encoded bits
  uint8_t tsbk_dibit[98];

  memset (tsbkbit, 0, sizeof(tsbkbit));
  memset (tsbk_dibit, 0, sizeof(tsbk_dibit));

  int dibit = 0;

  uint8_t tsbk_byte[12]; //12 byte return from p25_12
  memset (tsbk_byte, 0, sizeof(tsbk_byte));

  uint8_t r34byte_b[18];
  memset (r34byte_b, 0, sizeof(r34byte_b));

  uint8_t r34bytes[18*10]; //18 octets at 10 blocks (unsure of maximum needed, going with 10?)
  memset (r34bytes, 0, sizeof(r34bytes));

  int r34 = 0;

  //these expansions may be overkill, will need to look at them
  //and find optimal values for them at a later time
  unsigned long long int PDU[24*10];
  memset (PDU, 0, sizeof(PDU));

  int tsbk_decoded_bits[190*10]; //decoded bits from tsbk_bytes for sending to crc16_lb_bridge
  memset (tsbk_decoded_bits, 0, sizeof(tsbk_decoded_bits));

  uint8_t mpdu_decoded_bits[288*20];
  memset (mpdu_decoded_bits, 0, sizeof(mpdu_decoded_bits));

  uint8_t mpdu_crc_bits[196*20]; 
  memset (mpdu_crc_bits, 0, sizeof(mpdu_crc_bits));

  uint8_t mpdu_crc_bytes[24*10];
  memset (mpdu_crc_bytes, 0, sizeof(mpdu_crc_bytes));

  int i, j, k, x;
  int ec[10]; //error value returned from p25_12 or 34
  int err[2]; //error value returned from ccrc16 or crc32
  memset (ec, -2, sizeof(ec));
  memset (err, -2, sizeof(err));

  int skipdibit = 14; //initial status dibit will occur at 14, then add 36 each time it occurs
  int MFID = 0xFF; //Manufacturer ID

  uint8_t mpdu_byte[36];
  memset (mpdu_byte, 0, sizeof(mpdu_byte));

  uint8_t an = 0;
  uint8_t io = 0; 
  uint8_t fmt = 0;  
  uint8_t sap = 0; 
  uint8_t blks = 0; 
  uint8_t opcode = 0;

  uint32_t target1 = 0;
  uint32_t target2 = 0; //enchanced addressing value
  uint8_t fmf = 0;
  uint8_t pad = 0;
  uint8_t ns = 0;
  uint8_t fsnf = 0;
  uint8_t offset = 0;

  int end = 3; //ending value for data gathering repititions

  //CRC32
  uint32_t CRCComputed = 0;
  uint32_t CRCExtracted = 0;

  //collect x-len reps of 101 dibits (98 valid dibits with status dibits interlaced)
  for (j = 0; j < end; j++)
  {
    k = 0;
    for (i = 0; i < 101; i++)
    {

      dibit = getDibit(opts, state);

      if (i+(j*101) != skipdibit) //
      {
        tsbk_dibit[k] =   dibit;
        tsbkbit[k*2]   = (dibit >> 1) & 1;
        tsbkbit[k*2+1] = (dibit & 1);
        k++;
      }

      if (i+(j*101) == skipdibit) 
      {
        skipdibit += 36; //was set to 35, this was the bug
      }

    }

    //send header to p25_12 and return tsbk_byte
    if (j == 0) ec[j] = p25_12 (tsbk_dibit, tsbk_byte);

    //look at the header now and determine whether or not to 
    //decode using 34 or 12 rate decoders
    else if (r34)
    {
      ec[j] = dmr_34 (tsbk_dibit, r34byte_b);

      //shuffle 34 rate data into array
      if (j != 0) //should never happen, but just in case
        memcpy (r34bytes+((j-1)*18), r34byte_b, sizeof(r34byte_b));
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

    //check header data to see if this is a 12 rate trunking setup, or
    //34 rate packet data unit
    if (j == 0 && err[0] == 0)
    {
      an   = (mpdu_byte[0] >> 6) & 0x1;
      io   = (mpdu_byte[0] >> 5) & 0x1;
      fmt  = mpdu_byte[0] & 0x1F;
      sap  = mpdu_byte[1] & 0x3F;
      blks = mpdu_byte[6] & 0x7F;
      MFID = mpdu_byte[2];
      target1 = (mpdu_byte[3] << 16) | (mpdu_byte[4] << 8) | mpdu_byte[5]; //LLID, inbound is the source, outbound is the target
      target2 = (mpdu_byte[6] << 16) | (mpdu_byte[7] << 8) | mpdu_byte[8]; //unknown which octets convey the secondary address value

      fmf = (mpdu_byte[6] >> 7) & 0x1;
      pad = mpdu_byte[7] & 0x1F;
      ns = (mpdu_byte[8] >> 4) & 0x7;
      fsnf = mpdu_byte[8] & 0xF;
      offset = mpdu_byte[9] & 0x3F;

      if (an == 1 && fmt == 0b10110) //confirmed data packet header block
        r34 = 1;

      //set end value to number of blocks + 1 (block gathering for variable len)
      end = blks + 1;

      //sanity check
      if (end > 10) end = 10;  //TODO: Increase storage for handling these (unsure of upper limit)

      //old method for figuring this out, still unclear in the manual which is considered 12 rate, and which are 34 rate
      // if ((sap == 0x3D) && ((fmt == 0x17) || (fmt == 0x15))) ; //MBT format 12 rate
      // else r34 = 1; //set flag to decode as 34 rate data PDU (confirmed)
    }
    
  }

  //group list mode so we can look and see if we need to block tuning any groups, etc
  char mode[8]; //allow, block, digital, enc, etc
  sprintf (mode, "%s", "");

  //if we are using allow/whitelist mode, then write 'B' to mode for block
  //comparison below will look for an 'A' to write to mode if it is allowed
  if (opts->trunk_use_allow_list == 1) sprintf (mode, "%s", "B");

  if (err[0] == 0)
  {
    fprintf (stderr, "%s",KGRN);
    fprintf (stderr, " P25 Data - AN: %d; IO: %d; FMT: %02X; SAP: 0x%02X;", an, io, fmt, sap);
    processSAP(sap); //decode SAP to see what kind of data we are dealing with

    fprintf (stderr, "\n F: %d; Blocks: %02X; Pad: %d; NS: %d; FSNF: %d; Offset: %d; MFID: %02X;", fmf, blks, pad, ns, fsnf, offset, MFID);
    if (io == 1 && sap != 61 && sap != 63) //destination address if IO bit set
      fprintf (stderr, " Target: %d;", target1);

    // if (sap == 31) //still unclear on which octets or SAP convey this address, this sap is for extended address (same thing?)
    //   fprintf (stderr, " Source: %d;", target2); //unsure if this is a source address, manual just says a second address
    UNUSED(target2);

    //TODO: Print to Data Call String for Ncurses Terminal
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
    for (i = 0; i < 196; i++) mpdu_crc_bits[i] = mpdu_decoded_bits[i+96];
    for (i = 0; i < 24; i++) mpdu_crc_bytes[i] = mpdu_byte[i+12];
    if (blks == 1) //blocks to follow is 1
    {
      CRCExtracted = (uint32_t)ConvertBitIntoBytes(&mpdu_crc_bits[96-32], 32);
      CRCComputed  = crc32mbf(mpdu_crc_bytes, 96-32);
      if (CRCComputed == CRCExtracted) err[1] = 0;
    }
    if (blks == 2) //blocks to follow is 2
    {
      CRCExtracted = (uint32_t)ConvertBitIntoBytes(&mpdu_crc_bits[192-32], 32);
      CRCComputed  = crc32mbf(mpdu_crc_bytes, 192-32);
      if (CRCComputed == CRCExtracted) err[1] = 0;
    }

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
            //testing switch to P2 channel symbol rate with qpsk enabled, we need to know if we are going to a TDMA channel or an FDMA channel
            // if (opts->mod_qpsk == 1)
            // {
            // 	int spacing = state->p25_chan_spac[channelt >> 12];
            // 	if (spacing == 0x64) //tdma should always be 0x64, and fdma should always be 0x32
            // 	{
            // 		state->samplesPerSymbol = 8;
            // 		state->symbolCenter = 3;
            // 	}	
            // }

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
        long int source = (mpdu_byte[3] << 16) |(mpdu_byte[4] << 8) | mpdu_byte[5];
        long int target = (mpdu_byte[19] << 16) |(mpdu_byte[20] << 8) | mpdu_byte[21];
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
        fprintf (stderr, "\n  SVC [%02X] CHAN-T [%04X] CHAN-R [%04X] Source [%ld][%04lX] Target [%ld][%04lX]", svc, channelt, channelr, source, source, target, target);
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

            //testing switch to P2 channel symbol rate with qpsk enabled, we need to know if we are going to a TDMA channel or an FDMA channel
            // if (opts->mod_qpsk == 1)
            // {
            // 	int spacing = state->p25_chan_spac[channelt >> 12];
            // 	if (spacing == 0x64) //tdma should always be 0x64, and fdma should always be 0x32
            // 	{
            // 		state->samplesPerSymbol = 8;
            // 		state->symbolCenter = 3;
            // 	}	
            // }

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

        //TG hold on UU_V -- will want to disable UU_V grants while TG Hold enabled -- same for Telephone?
        if (state->tg_hold != 0 && state->tg_hold != target) sprintf (mode, "%s", "B");
        // if (state->tg_hold != 0 && state->tg_hold == target)
        // {
        // 	sprintf (mode, "%s", "A");
        // 	opts->p25_is_tuned = 0; //unlock tuner
        // }

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
        //convert mpdu_byte to vPDU format and get indication of what the message was
        PDU[0] = 0x0C; //P25p1 MB Duid 0x0C
        PDU[1] = opcode;
        PDU[2] = MFID;
        PDU[1] = PDU[1] ^ 0xC0; //flip bits to make it compatible with MAC_PDUs explicit format

        //without any extra bytes filled in, we can atleast see what the MBT is supposed to be
        //Hopefully, no 0 value things will cause issues -- call grants, iden_up, etc!
        // fprintf (stderr, "%s",KYEL);
        // process_MAC_VPDU(opts, state, 0, PDU);
        // fprintf (stderr, "%s",KNRM);
      }
    }

    SKIPCALL: ; //do nothing

    if (opts->payload == 1)
    {
      fprintf (stderr, "%s",KCYN);
      fprintf (stderr, "\n P25 MBT Payload \n  ");
      for (i = 0; i < 36; i++)
      {
        fprintf (stderr, "[%02X]", mpdu_byte[i]);
        if (i == 11 || i == 23) fprintf (stderr, "\n  ");

      }
      fprintf (stderr, " CRC EXT %08X CMP %08X", CRCExtracted, CRCComputed);
      fprintf (stderr, "%s ", KNRM);
      fprintf (stderr, "\n "); 

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

    //WIP: CRC32 (skip the CRC9, not really needed when you can just do the completed one) 
    // for (i = 0; i < 196; i++) mpdu_crc_bits[i] = mpdu_decoded_bits[i+96+16];
    // for (i = 0; i < 16; i++) mpdu_crc_bytes[i] = mpdu_byte[i+12+2];

    // if (blks == 1) //blocks to follow is 1
    // {
    //   CRCExtracted = (uint32_t)ConvertBitIntoBytes(&mpdu_crc_bits[96-32], 32);
    //   CRCComputed  = crc32mbf(mpdu_crc_bytes, 96-32);
    //   if (CRCComputed == CRCExtracted) err[1] = 0;
    // }
    // if (blks == 2) //blocks to follow is 2
    // {
    //   CRCExtracted = (uint32_t)ConvertBitIntoBytes(&mpdu_crc_bits[192-32], 32);
    //   CRCComputed  = crc32mbf(mpdu_crc_bytes, 192-32);
    //   if (CRCComputed == CRCExtracted) err[1] = 0;
    // }

    
    //TODO: Combine both of these into one PDU bit set and run CRC on it, etc
    //TODO: Handle the Confirmed Data Aspects of this format (CRC9 and DBSN)
    //TOOD: Decode some LRRP and/or Text Message Formats, etc
    // if (opts->payload == 1)
    {
      fprintf (stderr, "%s",KCYN);
      fprintf (stderr, "\n P25 MPDU Payload \n  ");
      for (i = 0; i < 12; i++) //header
        fprintf (stderr, "[%02X]", mpdu_byte[i]);
      fprintf (stderr, " Header \n  ");

      //variable len printing
      for (i = 0; i < 18*blks; i++)
      {
        if (i == 18 || i == 36 || i == 54 || i == 72 || i == 90)
          fprintf (stderr, "\n  ");
        fprintf (stderr, "[%02X]", r34bytes[i]);
      }

      // if (err[1] != 0) 
      // {
      //   fprintf (stderr, "%s",KRED);
      //   fprintf (stderr, " (MPDU CRC32 ERR)");
      //   fprintf (stderr, "%s",KCYN);
      //   fprintf (stderr, " CRC EXT %08X CMP %08X", CRCExtracted, CRCComputed); //TODO
      // }

      fprintf (stderr, "\n P25 MPDU ASCII: ");


      for (i = 2; i < (18*blks)-4; i++) //skip the first DBSN/CRC9 and Final CRC32
      {
        if (i == 16 || i == 32 || i == 48 || i == 60 || i == 72)
          i += 2; // skip the next DBSN/CRC9 values
        if (r34bytes[i] <= 0x7E && r34bytes[i] >=0x20)
        {
          fprintf (stderr, "%c", r34bytes[i]);
        }
        else fprintf (stderr, " ");
      }

      fprintf (stderr, "%s ", KNRM);
      fprintf (stderr, "\n");      

    }
  } //end r34
  else if (err[0] == 0)
  {
    //just print the header
    // if (opts->payload == 1)
    {
      fprintf (stderr, "%s",KCYN);
      fprintf (stderr, "\n P25 MPDU Header: ");
      for (i = 0; i < 12; i++) //header
        fprintf (stderr, "[%02X]", mpdu_byte[i]);
      fprintf (stderr, "%s",KNRM);
      fprintf (stderr, "\n");
    }
  }
  else fprintf (stderr, "\n");
}
